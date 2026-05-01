#include "decision/mission_behavior_tree.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "decision/action.h"
#include "decision/condition.h"
#include "decision/selector.h"
#include "decision/sequence.h"

namespace sensor_fusion::decision {
namespace {

struct TickContext {
  BlackboardSnapshot snapshot;
  std::optional<TrackFact> engage_target;
  std::optional<TrackFact> tracking_target;
  std::optional<InterceptorFact> interceptor;
  BtTickResult result;
  bool engagement_denied = false;
};

double distance_from_origin(const std::array<double, 3>& pos) {
  return std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
}

bool has_active_engagement_for_target(const BlackboardSnapshot& snapshot,
                                      sensor_fusion::TrackId track_id) {
  for (const auto& interceptor : snapshot.interceptors) {
    if (snapshot.engagement.active && snapshot.engagement.track_id == track_id &&
        interceptor.interceptor_id == snapshot.engagement.interceptor_id &&
        interceptor.engaged && interceptor.target_id == track_id) {
      return true;
    }
    if (interceptor.engaged && interceptor.target_id == track_id) {
      return true;
    }
  }
  return false;
}

void record(TickContext& ctx,
            const std::string& node,
            Status status,
            const std::string& detail) {
  ctx.result.node_trace.push_back(BtNodeTrace{
      .node = node,
      .status = status,
      .detail = detail,
  });
}

std::unique_ptr<Node> condition_node(TickContext& ctx,
                                     const std::string& name,
                                     std::function<bool()> predicate,
                                     std::function<std::string()> detail) {
  return std::make_unique<Condition>([&ctx, name, predicate = std::move(predicate),
                                      detail = std::move(detail)]() {
    const bool ok = predicate();
    const Status status = ok ? Status::Success : Status::Failure;
    record(ctx, name, status, detail());
    return ok;
  });
}

std::unique_ptr<Node> action_node(TickContext& ctx,
                                  const std::string& name,
                                  std::function<Status()> action,
                                  std::function<std::string()> detail) {
  return std::make_unique<Action>([&ctx, name, action = std::move(action),
                                   detail = std::move(detail)]() {
    const Status status = action();
    record(ctx, name, status, detail());
    return status;
  });
}

DecisionEvent event_for_track(const TrackFact& track,
                              std::string decision_type,
                              std::string reason) {
  return DecisionEvent{
      .track_id = track.track_id,
      .decision_type = std::move(decision_type),
      .reason = std::move(reason),
  };
}

}  // namespace

MissionBehaviorTree::MissionBehaviorTree(DecisionConfig config) : config_(config) {}

BtTickResult MissionBehaviorTree::tick(MissionBlackboard& blackboard) const {
  TickContext ctx;
  ctx.snapshot = blackboard.snapshot();
  ctx.result.tick = ctx.snapshot.tick;
  ctx.result.time_s = ctx.snapshot.time_s;
  ctx.result.mode = MissionMode::Search;
  ctx.result.event = DecisionEvent{
      .track_id = sensor_fusion::TrackId(0),
      .decision_type = "search",
      .reason = "waiting_for_detections",
  };

  auto engage_sequence = std::make_unique<Sequence>();
  engage_sequence->add_child(condition_node(
      ctx, "engage.target_ready",
      [&]() {
        ctx.engage_target = select_highest_priority_track(ctx.snapshot, true);
        if (!ctx.engage_target.has_value()) {
          return false;
        }
        return ctx.engage_target->score > config_.engage_score_threshold;
      },
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return "no_confirmed_track";
        }
        if (ctx.engage_target->score <= config_.engage_score_threshold) {
          return "score_below_threshold";
        }
        return "confirmed_track_ready";
      }));
  engage_sequence->add_child(condition_node(
      ctx, "engage.confidence_allowed",
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return false;
        }
        if (ctx.engage_target->confidence < config_.min_confidence_to_engage) {
          ctx.engagement_denied = true;
          ctx.result.event =
              event_for_track(*ctx.engage_target, "engage_denied", "low_confidence");
          return false;
        }
        return true;
      },
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return "no_target";
        }
        return ctx.engage_target->confidence < config_.min_confidence_to_engage
                   ? "low_confidence"
                   : "confidence_ok";
      }));
  engage_sequence->add_child(condition_node(
      ctx, "engage.system_health",
      [&]() {
        if (has_stale_required_sensor(ctx.snapshot)) {
          ctx.engagement_denied = true;
          if (ctx.engage_target.has_value()) {
            ctx.result.event = event_for_track(
                *ctx.engage_target, "engage_denied", "sensor_health_degraded");
          }
          return false;
        }
        return true;
      },
      [&]() {
        return has_stale_required_sensor(ctx.snapshot) ? "sensor_health_degraded"
                                                       : "sensors_nominal";
      }));
  engage_sequence->add_child(condition_node(
      ctx, "engage.range_allowed",
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return false;
        }
        const double range = distance_from_origin(ctx.engage_target->position);
        if (range < config_.no_engage_zone_radius_m) {
          ctx.engagement_denied = true;
          ctx.result.event = event_for_track(
              *ctx.engage_target, "engage_denied", "inside_no_engage_zone");
          return false;
        }
        if (range > config_.max_engagement_range_m) {
          ctx.engagement_denied = true;
          ctx.result.event = event_for_track(
              *ctx.engage_target, "engage_denied", "outside_max_engagement_range");
          return false;
        }
        return true;
      },
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return "no_target";
        }
        const double range = distance_from_origin(ctx.engage_target->position);
        if (range < config_.no_engage_zone_radius_m) {
          return "inside_no_engage_zone";
        }
        if (range > config_.max_engagement_range_m) {
          return "outside_max_engagement_range";
        }
        return "range_ok";
      }));

  auto active_engagement_sequence = std::make_unique<Sequence>();
  active_engagement_sequence->add_child(condition_node(
      ctx, "engage.active_engagement_exists",
      [&]() {
        return ctx.engage_target.has_value() &&
               has_active_engagement_for_target(ctx.snapshot, ctx.engage_target->track_id);
      },
      [&]() {
        return ctx.engage_target.has_value() &&
                       has_active_engagement_for_target(ctx.snapshot, ctx.engage_target->track_id)
                   ? "engagement_active"
                   : "no_active_engagement";
      }));
  active_engagement_sequence->add_child(action_node(
      ctx, "engage.maintain_engagement",
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return Status::Failure;
        }
        ctx.result.mode = MissionMode::Engage;
        ctx.result.event =
            event_for_track(*ctx.engage_target, "engage", "engagement_active");
        blackboard.set_mode(MissionMode::Engage);
        return Status::Success;
      },
      [&]() { return "engagement_state_active"; }));

  auto new_assignment_sequence = std::make_unique<Sequence>();
  new_assignment_sequence->add_child(condition_node(
      ctx, "engage.interceptor_available",
      [&]() {
        ctx.interceptor = select_available_interceptor(ctx.snapshot);
        if (!ctx.interceptor.has_value()) {
          ctx.engagement_denied = true;
          if (ctx.engage_target.has_value()) {
            ctx.result.event = event_for_track(
                *ctx.engage_target, "engage_denied", "interceptor_unavailable");
          }
          return false;
        }
        return true;
      },
      [&]() {
        return ctx.interceptor.has_value() ? "interceptor_available"
                                           : "interceptor_unavailable";
      }));
  new_assignment_sequence->add_child(action_node(
      ctx, "engage.assign_interceptor",
      [&]() {
        if (!ctx.engage_target.has_value() || !ctx.interceptor.has_value()) {
          return Status::Failure;
        }
        ctx.result.engagement_commands.push_back(EngagementCommand{
            .interceptor_id = ctx.interceptor->interceptor_id,
            .track_id = ctx.engage_target->track_id,
            .target_position = ctx.engage_target->position,
            .reason = "bt_engage_target",
        });
        ctx.result.mode = MissionMode::Engage;
        ctx.result.event =
            event_for_track(*ctx.engage_target, "engage", "safety_checks_passed");
        return Status::Success;
      },
      [&]() {
        return ctx.engage_target.has_value() ? "assignment_emitted" : "no_target";
      }));
  new_assignment_sequence->add_child(action_node(
      ctx, "engage.update_engagement_state",
      [&]() {
        if (!ctx.engage_target.has_value() || !ctx.interceptor.has_value()) {
          return Status::Failure;
        }
        blackboard.set_engagement(ctx.interceptor->interceptor_id, ctx.engage_target->track_id);
        return Status::Success;
      },
      [&]() { return "engagement_state_active"; }));

  auto engage_control = std::make_unique<Selector>();
  engage_control->add_child(std::move(active_engagement_sequence));
  engage_control->add_child(std::move(new_assignment_sequence));
  engage_sequence->add_child(std::move(engage_control));

  auto track_sequence = std::make_unique<Sequence>();
  track_sequence->add_child(condition_node(
      ctx, "track.track_exists",
      [&]() {
        ctx.tracking_target = select_highest_priority_track(ctx.snapshot, false);
        return ctx.tracking_target.has_value();
      },
      [&]() {
        return ctx.tracking_target.has_value() ? "track_available" : "no_track";
      }));
  track_sequence->add_child(action_node(
      ctx, "track.maintain_tracking",
      [&]() {
        if (!ctx.tracking_target.has_value()) {
          return Status::Failure;
        }
        ctx.result.mode = MissionMode::Track;
        if (!ctx.engagement_denied) {
          if (ctx.tracking_target->status ==
              sensor_fusion::fusion_core::TrackStatus::Tentative) {
            ctx.result.event =
                event_for_track(*ctx.tracking_target, "acquire", "tentative_wait");
          } else if (ctx.tracking_target->score <= config_.engage_score_threshold) {
            ctx.result.event =
                event_for_track(*ctx.tracking_target, "idle", "score_below_threshold");
          } else {
            ctx.result.event =
                event_for_track(*ctx.tracking_target, "track", "maintain_tracking");
          }
        }
        blackboard.set_mode(MissionMode::Track);
        return Status::Success;
      },
      [&]() { return "tracking_state_active"; }));

  auto search_sequence = std::make_unique<Sequence>();
  search_sequence->add_child(action_node(
      ctx, "search.scan_wait",
      [&]() {
        if (!ctx.engagement_denied) {
          ctx.result.mode = MissionMode::Search;
          ctx.result.event = DecisionEvent{
              .track_id = sensor_fusion::TrackId(0),
              .decision_type = "search",
              .reason = "waiting_for_detections",
          };
        }
        blackboard.set_mode(ctx.result.mode);
        return Status::Success;
      },
      [&]() { return ctx.engagement_denied ? "engagement_denied" : "scan_wait"; }));

  Selector root;
  root.add_child(std::move(engage_sequence));
  root.add_child(std::move(track_sequence));
  root.add_child(std::move(search_sequence));
  const Status root_status = root.tick();
  record(ctx, "root.selector", root_status, mission_mode_to_string(ctx.result.mode));

  return ctx.result;
}

const char* bt_status_to_string(Status status) {
  switch (status) {
    case Status::Success:
      return "success";
    case Status::Failure:
      return "failure";
    case Status::Running:
      return "running";
  }
  return "failure";
}

}  // namespace sensor_fusion::decision
