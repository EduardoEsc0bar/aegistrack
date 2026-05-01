#include "decision/mission_behavior_tree.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "decision/action.h"
#include "decision/condition.h"
#include "decision/engagement_scoring.h"
#include "decision/selector.h"
#include "decision/sequence.h"
#include "observability/metrics.h"

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

std::optional<TrackFact> find_track(const BlackboardSnapshot& snapshot,
                                    sensor_fusion::TrackId track_id) {
  for (const auto& track : snapshot.tracks) {
    if (track.track_id == track_id &&
        track.status != sensor_fusion::fusion_core::TrackStatus::Deleted) {
      return track;
    }
  }
  return std::nullopt;
}

bool interceptor_engaged_with_track(const BlackboardSnapshot& snapshot,
                                    uint64_t interceptor_id,
                                    sensor_fusion::TrackId track_id) {
  for (const auto& interceptor : snapshot.interceptors) {
    if (interceptor.interceptor_id == interceptor_id && interceptor.engaged &&
        interceptor.target_id == track_id) {
      return true;
    }
  }
  return false;
}

void record_event(TickContext& ctx,
                  std::string event,
                  sensor_fusion::TrackId track_id,
                  uint64_t interceptor_id,
                  std::string reason) {
  ctx.result.events.push_back(BtEventTrace{
      .event = std::move(event),
      .track_id = track_id,
      .interceptor_id = interceptor_id,
      .reason = std::move(reason),
  });
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

MissionBehaviorTree::MissionBehaviorTree(DecisionConfig config,
                                         sensor_fusion::observability::Metrics* metrics)
    : config_(config), metrics_(metrics) {}

BtTickResult MissionBehaviorTree::tick(MissionBlackboard& blackboard) {
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

  auto active_index_for_track = [&](sensor_fusion::TrackId track_id) -> size_t {
    for (size_t i = 0; i < memory_.active_engagements.size(); ++i) {
      if (memory_.active_engagements[i].track_id == track_id) {
        return i;
      }
    }
    return memory_.active_engagements.size();
  };

  auto add_or_refresh_active = [&](sensor_fusion::TrackId track_id, uint64_t interceptor_id) {
    const size_t existing = active_index_for_track(track_id);
    if (existing < memory_.active_engagements.size()) {
      memory_.active_engagements[existing].interceptor_id = interceptor_id;
      return;
    }
    memory_.active_engagements.push_back(MissionMemory::ActiveEngagement{
        .track_id = track_id,
        .interceptor_id = interceptor_id,
        .engagement_start_time_s = ctx.snapshot.time_s,
        .last_command_time_s = ctx.snapshot.time_s,
    });
  };

  auto find_track_any_status = [&](sensor_fusion::TrackId track_id) -> std::optional<TrackFact> {
    for (const auto& track : ctx.snapshot.tracks) {
      if (track.track_id == track_id) {
        return track;
      }
    }
    return std::nullopt;
  };

  auto find_interceptor = [&](uint64_t interceptor_id) -> std::optional<InterceptorFact> {
    for (const auto& interceptor : ctx.snapshot.interceptors) {
      if (interceptor.interceptor_id == interceptor_id) {
        return interceptor;
      }
    }
    return std::nullopt;
  };

  if (ctx.snapshot.engagement.active) {
    add_or_refresh_active(ctx.snapshot.engagement.track_id,
                          ctx.snapshot.engagement.interceptor_id);
  }
  for (const auto& interceptor : ctx.snapshot.interceptors) {
    if (interceptor.engaged && interceptor.target_id.value != 0) {
      add_or_refresh_active(interceptor.target_id, interceptor.interceptor_id);
    }
  }

  auto record_reconciled_engagement = [&](const MissionMemory::ActiveEngagement& active,
                                          const std::string& reason) {
    ++ctx.result.reconciliation_removals;
    record_event(ctx, "disengage", active.track_id, active.interceptor_id, reason);
    if (metrics_ != nullptr) {
      metrics_->inc("bt_engagement_reconciled_total");
      metrics_->inc("bt_stale_engagement_removed_total");
      if (reason == "track_missing") {
        metrics_->inc("bt_disengage_track_missing_total");
      } else if (reason == "track_deleted") {
        metrics_->inc("bt_disengage_track_deleted_total");
      } else if (reason == "interceptor_missing") {
        metrics_->inc("bt_disengage_interceptor_missing_total");
      } else if (reason == "interceptor_idle") {
        metrics_->inc("bt_disengage_interceptor_idle_total");
      } else if (reason == "interceptor_retargeted") {
        metrics_->inc("bt_disengage_interceptor_retargeted_total");
      } else if (reason == "completed_or_killed") {
        metrics_->inc("bt_disengage_completed_or_killed_total");
      } else {
        metrics_->inc("bt_disengage_stale_engagement_total");
      }
    }
  };

  std::vector<MissionMemory::ActiveEngagement> reconciled_engagements;
  reconciled_engagements.reserve(memory_.active_engagements.size());
  for (const auto& active : memory_.active_engagements) {
    const auto track = find_track_any_status(active.track_id);
    const auto interceptor = find_interceptor(active.interceptor_id);
    std::string removal_reason;
    if (!track.has_value()) {
      removal_reason = interceptor.has_value() && !interceptor->engaged
                           ? "completed_or_killed"
                           : "track_missing";
    } else if (track->status == sensor_fusion::fusion_core::TrackStatus::Deleted) {
      removal_reason = interceptor.has_value() && !interceptor->engaged
                           ? "completed_or_killed"
                           : "track_deleted";
    } else if (!interceptor.has_value()) {
      removal_reason = "interceptor_missing";
    } else if (!interceptor->engaged || interceptor->target_id.value == 0) {
      removal_reason = "interceptor_idle";
    } else if (interceptor->target_id != active.track_id) {
      removal_reason = "interceptor_retargeted";
    }

    if (!removal_reason.empty()) {
      record_reconciled_engagement(active, removal_reason);
      if (ctx.snapshot.engagement.active &&
          ctx.snapshot.engagement.track_id == active.track_id &&
          ctx.snapshot.engagement.interceptor_id == active.interceptor_id) {
        blackboard.clear_engagement();
      }
      continue;
    }
    reconciled_engagements.push_back(active);
  }
  memory_.active_engagements = std::move(reconciled_engagements);
  ctx.result.active_engagement_count_after_reconcile =
      static_cast<uint32_t>(memory_.active_engagements.size());
  if (metrics_ != nullptr) {
    metrics_->set_gauge("bt_active_engagements_after_reconcile",
                        static_cast<double>(memory_.active_engagements.size()));
  }

  std::vector<MissionMemory::TrackStability> next_stability;
  next_stability.reserve(ctx.snapshot.tracks.size());
  for (const auto& track : ctx.snapshot.tracks) {
    if (track.status != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
      continue;
    }
    uint32_t ticks = 1;
    for (const auto& previous : memory_.stable_tracks) {
      if (previous.track_id == track.track_id) {
        ticks = previous.ticks + 1;
        break;
      }
    }
    next_stability.push_back(MissionMemory::TrackStability{
        .track_id = track.track_id,
        .ticks = ticks,
    });
  }
  memory_.stable_tracks = std::move(next_stability);

  auto stable_ticks_for = [&](sensor_fusion::TrackId track_id) -> uint32_t {
    for (const auto& stable : memory_.stable_tracks) {
      if (stable.track_id == track_id) {
        return stable.ticks;
      }
    }
    return 0;
  };

  auto track_is_stable = [&](sensor_fusion::TrackId track_id) {
    return stable_ticks_for(track_id) >= config_.stable_track_ticks_to_engage;
  };

  auto track_is_engaged = [&](sensor_fusion::TrackId track_id) {
    return active_index_for_track(track_id) < memory_.active_engagements.size();
  };

  auto track_is_engageable = [&](const TrackFact& track) {
    if (track.status != sensor_fusion::fusion_core::TrackStatus::Confirmed) {
      return false;
    }
    if (track.score <= config_.engage_score_threshold ||
        track.confidence < config_.min_confidence_to_engage) {
      return false;
    }
    if (has_stale_required_sensor(ctx.snapshot)) {
      return false;
    }
    const double range = distance_from_origin(track.position);
    if (range < config_.no_engage_zone_radius_m ||
        range > config_.max_engagement_range_m) {
      return false;
    }
    if (memory_.last_denied_track_id == track.track_id &&
        ctx.snapshot.time_s < memory_.denial_cooldown_until_s) {
      return false;
    }
    return track_is_stable(track.track_id);
  };

  const EngagementScoringConfig scoring_config{
      .priority_weight = config_.engagement_priority_weight,
      .confidence_weight = config_.engagement_confidence_weight,
      .intercept_time_weight = config_.engagement_intercept_time_weight,
      .distance_weight = config_.engagement_distance_weight,
      .distance_normalizer_m = config_.engagement_distance_normalizer_m,
      .default_interceptor_speed_mps = config_.default_interceptor_speed_mps,
  };

  auto score_pair = [&](const TrackFact& track,
                        const InterceptorFact& interceptor,
                        bool already_engaged) {
    return score_engagement_pair(track, interceptor, scoring_config, already_engaged);
  };

  const auto best_confirmed = select_highest_priority_track(ctx.snapshot, true);
  if (best_confirmed.has_value()) {
    memory_.stable_track_id = best_confirmed->track_id;
    memory_.consecutive_track_ticks = stable_ticks_for(best_confirmed->track_id);
  } else {
    memory_.stable_track_id = sensor_fusion::TrackId(0);
    memory_.consecutive_track_ticks = 0;
  }

  std::vector<InterceptorFact> idle_interceptors;
  for (const auto& interceptor : ctx.snapshot.interceptors) {
    if (interceptor.available && !interceptor.engaged) {
      idle_interceptors.push_back(interceptor);
    }
  }
  ctx.result.idle_interceptor_count = static_cast<uint32_t>(idle_interceptors.size());

  auto finalize = [&](MissionMode mode) {
    ctx.result.mode = mode;
    ctx.result.active_engagement_count =
        static_cast<uint32_t>(memory_.active_engagements.size());
    if (metrics_ != nullptr) {
      metrics_->set_gauge("bt_consecutive_track_ticks",
                          static_cast<double>(memory_.consecutive_track_ticks));
      metrics_->set_gauge("bt_active_engagements",
                          static_cast<double>(memory_.active_engagements.size()));
      metrics_->set_gauge("bt_engaged_targets",
                          static_cast<double>(memory_.active_engagements.size()));
      for (const auto& active : memory_.active_engagements) {
        metrics_->observe("bt_active_engagement_age_s",
                          std::max(0.0, ctx.snapshot.time_s -
                                            active.engagement_start_time_s));
      }
    }
    if (mode != memory_.last_mode) {
      if (metrics_ != nullptr) {
        metrics_->inc("bt_mode_transition_total");
      }
      record_event(ctx, "mode_transition", ctx.result.event.track_id, 0,
                   std::string(mission_mode_to_string(memory_.last_mode)) + "_to_" +
                       mission_mode_to_string(mode));
    }
    memory_.last_mode = mode;
    return ctx.result;
  };

  bool has_stable_unengaged_target = false;
  for (const auto& track : ctx.snapshot.tracks) {
    if (!track_is_engaged(track.track_id) && track_is_engageable(track)) {
      has_stable_unengaged_target = true;
      break;
    }
  }

  bool maintained_engagement = false;
  for (auto it = memory_.active_engagements.begin();
       it != memory_.active_engagements.end();) {
    const auto active_track = find_track(ctx.snapshot, it->track_id);
    const bool active_interceptor_engaged =
        interceptor_engaged_with_track(ctx.snapshot, it->interceptor_id, it->track_id);

    if (active_track.has_value() && active_interceptor_engaged) {
      it->missing_ticks = 0;
      if (active_track->confidence < config_.min_confidence_to_engage) {
        ++it->low_confidence_ticks;
      } else {
        it->low_confidence_ticks = 0;
      }
      maintained_engagement = true;
      ctx.result.event = event_for_track(*active_track, "engage_maintain",
                                         "engagement_active");
      record(ctx, "engage.maintain_engagement", Status::Success,
             "engagement_state_active");
      record_event(ctx, "engage_maintain", active_track->track_id,
                   it->interceptor_id, "engagement_active");
      if (metrics_ != nullptr) {
        metrics_->inc("bt_engage_maintain_total");
        metrics_->inc("bt_duplicate_assignment_suppressed_total");
      }
      ++it;
      continue;
    }

    ++it->missing_ticks;
    if (it->missing_ticks >= config_.stable_track_ticks_to_engage) {
      record_event(ctx, "disengage", it->track_id, it->interceptor_id,
                   "active_track_or_interceptor_missing");
      it = memory_.active_engagements.erase(it);
      blackboard.clear_engagement();
    } else {
      ++it;
    }
  }

  while (!idle_interceptors.empty()) {
    struct AssignmentCandidate {
      size_t interceptor_index = 0;
      size_t track_index = 0;
      EngagementScore score;
    };
    std::optional<AssignmentCandidate> best_candidate;
    for (size_t interceptor_index = 0; interceptor_index < idle_interceptors.size();
         ++interceptor_index) {
      for (size_t track_index = 0; track_index < ctx.snapshot.tracks.size();
           ++track_index) {
        const auto& track = ctx.snapshot.tracks[track_index];
        if (track_is_engaged(track.track_id) || !track_is_engageable(track)) {
          continue;
        }
        const EngagementScore score =
            score_pair(track, idle_interceptors[interceptor_index], false);
        if (metrics_ != nullptr) {
          metrics_->inc("bt_candidate_engagement_pairs");
          metrics_->observe("bt_engagement_score", score.score);
          metrics_->observe("bt_estimated_intercept_time_s",
                            score.estimated_intercept_time_s);
        }
        if (!std::isfinite(score.score) ||
            !std::isfinite(score.estimated_intercept_time_s)) {
          if (metrics_ != nullptr) {
            metrics_->inc("bt_intercept_time_rejected_total");
          }
          continue;
        }
        if (!best_candidate.has_value() ||
            score.score > best_candidate->score.score) {
          best_candidate = AssignmentCandidate{
              .interceptor_index = interceptor_index,
              .track_index = track_index,
              .score = score,
          };
        }
      }
    }
    if (!best_candidate.has_value()) {
      break;
    }

    const auto interceptor = idle_interceptors[best_candidate->interceptor_index];
    const auto track = ctx.snapshot.tracks[best_candidate->track_index];
    const EngagementScore selected_score = best_candidate->score;
    idle_interceptors.erase(idle_interceptors.begin() +
                            static_cast<std::ptrdiff_t>(best_candidate->interceptor_index));
    const bool already_had_engagement = !memory_.active_engagements.empty();
    const std::string event_name =
        already_had_engagement ? "idle_interceptor_engage" : "engage_start";
    record(ctx, "engage.target_ready", Status::Success, "confirmed_track_ready");
    record(ctx, "engage.stability_gate", Status::Success,
           "stable_ticks_" + std::to_string(stable_ticks_for(track.track_id)));
    record(ctx, "engage.assign_interceptor", Status::Success, "assignment_emitted");
    ctx.result.engagement_commands.push_back(EngagementCommand{
        .interceptor_id = interceptor.interceptor_id,
        .track_id = track.track_id,
        .target_position = track.position,
        .reason = "intercept_aware_idle_assignment",
    });
    if (ctx.result.assignment_reason.empty()) {
      ctx.result.selected_track_id = track.track_id;
      ctx.result.selected_interceptor_id = interceptor.interceptor_id;
      ctx.result.selected_engagement_score = selected_score.score;
      ctx.result.selected_estimated_intercept_time_s =
          selected_score.estimated_intercept_time_s;
      ctx.result.assignment_reason = "intercept_aware_idle_assignment";
    }
    memory_.active_engagements.push_back(MissionMemory::ActiveEngagement{
        .track_id = track.track_id,
        .interceptor_id = interceptor.interceptor_id,
        .engagement_start_time_s = ctx.snapshot.time_s,
        .last_command_time_s = ctx.snapshot.time_s,
    });
    blackboard.set_engagement(interceptor.interceptor_id, track.track_id);
    ctx.result.event = event_for_track(track, "engage", event_name);
    record_event(ctx, event_name, track.track_id, interceptor.interceptor_id,
                 "intercept_aware_idle_assignment");
    if (metrics_ != nullptr) {
      metrics_->inc("bt_engage_start_total");
      metrics_->inc("bt_intercept_aware_assignment_total");
      metrics_->observe("bt_selected_engagement_score", selected_score.score);
      metrics_->observe("bt_selected_estimated_intercept_time_s",
                        selected_score.estimated_intercept_time_s);
      if (already_had_engagement) {
        metrics_->inc("bt_idle_interceptor_engage_total");
        metrics_->inc("bt_multi_engagement_total");
      }
    }
  }
  ctx.result.idle_interceptor_count = static_cast<uint32_t>(idle_interceptors.size());

  if (!ctx.result.engagement_commands.empty()) {
    ++memory_.consecutive_engage_ticks;
    return finalize(MissionMode::Engage);
  }

  if (maintained_engagement) {
    if (idle_interceptors.empty()) {
      struct RetaskCandidate {
        size_t active_index = 0;
        TrackFact candidate_track;
        EngagementScore candidate_score;
        EngagementScore active_score;
      };
      std::optional<RetaskCandidate> retask_candidate;
      for (size_t active_index = 0; active_index < memory_.active_engagements.size();
           ++active_index) {
        const auto active_track =
            find_track(ctx.snapshot, memory_.active_engagements[active_index].track_id);
        const auto interceptor =
            find_interceptor(memory_.active_engagements[active_index].interceptor_id);
        if (!active_track.has_value() || !interceptor.has_value()) {
          continue;
        }
        const EngagementScore active_score = score_pair(*active_track, *interceptor, false);
        for (const auto& track : ctx.snapshot.tracks) {
          if (track_is_engaged(track.track_id) || !track_is_engageable(track)) {
            continue;
          }
          const EngagementScore candidate_score = score_pair(track, *interceptor, false);
          if (metrics_ != nullptr) {
            metrics_->inc("bt_candidate_engagement_pairs");
            metrics_->observe("bt_engagement_score", candidate_score.score);
            metrics_->observe("bt_estimated_intercept_time_s",
                              candidate_score.estimated_intercept_time_s);
          }
          if (!std::isfinite(candidate_score.score) ||
              !std::isfinite(candidate_score.estimated_intercept_time_s)) {
            if (metrics_ != nullptr) {
              metrics_->inc("bt_intercept_time_rejected_total");
            }
            continue;
          }
          const bool better_than_current_best =
              !retask_candidate.has_value() ||
              candidate_score.score - active_score.score >
                  retask_candidate->candidate_score.score -
                      retask_candidate->active_score.score;
          if (better_than_current_best) {
            retask_candidate = RetaskCandidate{
                .active_index = active_index,
                .candidate_track = track,
                .candidate_score = candidate_score,
                .active_score = active_score,
            };
          }
        }
      }
      if (retask_candidate.has_value() && !memory_.active_engagements.empty()) {
        const double score_delta =
            retask_candidate->candidate_score.score - retask_candidate->active_score.score;
        if (score_delta >= config_.retask_engagement_score_margin) {
          auto& active = memory_.active_engagements[retask_candidate->active_index];
          const uint64_t interceptor_id = active.interceptor_id;
          ctx.result.engagement_commands.push_back(EngagementCommand{
              .interceptor_id = interceptor_id,
              .track_id = retask_candidate->candidate_track.track_id,
              .target_position = retask_candidate->candidate_track.position,
              .reason = "intercept_aware_retask",
          });
          ctx.result.selected_track_id = retask_candidate->candidate_track.track_id;
          ctx.result.selected_interceptor_id = interceptor_id;
          ctx.result.selected_engagement_score = retask_candidate->candidate_score.score;
          ctx.result.selected_estimated_intercept_time_s =
              retask_candidate->candidate_score.estimated_intercept_time_s;
          ctx.result.assignment_reason = "intercept_aware_retask";
          active.track_id = retask_candidate->candidate_track.track_id;
          active.engagement_start_time_s = ctx.snapshot.time_s;
          active.last_command_time_s = ctx.snapshot.time_s;
          active.missing_ticks = 0;
          active.low_confidence_ticks = 0;
          blackboard.set_engagement(interceptor_id,
                                    retask_candidate->candidate_track.track_id);
          ctx.result.event = event_for_track(retask_candidate->candidate_track, "engage",
                                             "retask_approved");
          record(ctx, "engage.retask_policy", Status::Success, "retask_approved");
          record_event(ctx, "retask_approved",
                       retask_candidate->candidate_track.track_id, interceptor_id,
                       "engagement_score_margin_exceeded");
          if (metrics_ != nullptr) {
            metrics_->inc("bt_retask_approved_total");
            metrics_->inc("bt_intercept_aware_assignment_total");
            metrics_->observe("bt_selected_engagement_score",
                              retask_candidate->candidate_score.score);
            metrics_->observe("bt_selected_estimated_intercept_time_s",
                              retask_candidate->candidate_score
                                  .estimated_intercept_time_s);
          }
          ++memory_.consecutive_engage_ticks;
          return finalize(MissionMode::Engage);
        }
        record(ctx, "engage.retask_policy", Status::Failure,
               "engagement_score_margin_not_exceeded");
        record_event(ctx, "retask_denied",
                     retask_candidate->candidate_track.track_id, 0,
                     "engagement_score_margin_not_exceeded");
        if (metrics_ != nullptr) {
          metrics_->inc("bt_retask_denied_total");
        }
        ctx.result.event.track_id = retask_candidate->candidate_track.track_id;
        ctx.result.event.reason = "retask_denied";
      }
    } else if (has_stable_unengaged_target && metrics_ != nullptr) {
      metrics_->inc("bt_interceptor_underutilized_ticks");
    }

    blackboard.set_mode(MissionMode::Engage);
    ++memory_.consecutive_engage_ticks;
    return finalize(MissionMode::Engage);
  }

  if (has_stable_unengaged_target && !idle_interceptors.empty() && metrics_ != nullptr) {
    metrics_->inc("bt_interceptor_underutilized_ticks");
  }

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
      ctx, "engage.denial_cooldown",
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return false;
        }
        const bool cooling_down =
            memory_.last_denied_track_id == ctx.engage_target->track_id &&
            ctx.snapshot.time_s < memory_.denial_cooldown_until_s;
        if (cooling_down) {
          ctx.engagement_denied = true;
          ctx.result.event =
              event_for_track(*ctx.engage_target, "track", "engage_cooldown");
          record_event(ctx, "engage_denied_cooldown", ctx.engage_target->track_id, 0,
                       memory_.last_denial_reason);
          if (metrics_ != nullptr) {
            metrics_->inc("bt_engage_cooldown_total");
            metrics_->observe("bt_denial_cooldown_s",
                              memory_.denial_cooldown_until_s - ctx.snapshot.time_s);
          }
          return false;
        }
        return true;
      },
      [&]() -> std::string {
        if (!ctx.engage_target.has_value()) {
          return "no_target";
        }
        if (memory_.last_denied_track_id == ctx.engage_target->track_id &&
            ctx.snapshot.time_s < memory_.denial_cooldown_until_s) {
          return "cooldown_active";
        }
        return "cooldown_clear";
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
      [&]() -> std::string {
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
      [&]() -> std::string {
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
  engage_sequence->add_child(condition_node(
      ctx, "engage.stability_gate",
      [&]() {
        if (!ctx.engage_target.has_value()) {
          return false;
        }
        return memory_.stable_track_id == ctx.engage_target->track_id &&
               memory_.consecutive_track_ticks >= config_.stable_track_ticks_to_engage;
      },
      [&]() -> std::string {
        if (!ctx.engage_target.has_value()) {
          return "no_target";
        }
        return memory_.stable_track_id == ctx.engage_target->track_id
                   ? "stable_ticks_" + std::to_string(memory_.consecutive_track_ticks)
                   : "different_stable_track";
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
            .reason = "bt_engage_start",
        });
        add_or_refresh_active(ctx.engage_target->track_id, ctx.interceptor->interceptor_id);
        const size_t active_index = active_index_for_track(ctx.engage_target->track_id);
        if (active_index < memory_.active_engagements.size()) {
          memory_.active_engagements[active_index].last_command_time_s = ctx.snapshot.time_s;
        }
        ctx.result.mode = MissionMode::Engage;
        ctx.result.event =
            event_for_track(*ctx.engage_target, "engage", "engage_start");
        record_event(ctx, "engage_start", ctx.engage_target->track_id,
                     ctx.interceptor->interceptor_id, "safety_checks_passed");
        if (metrics_ != nullptr) {
          metrics_->inc("bt_engage_start_total");
        }
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

  if (ctx.result.event.decision_type == "engage_denied") {
    memory_.last_denied_track_id = ctx.result.event.track_id;
    memory_.last_denial_reason = ctx.result.event.reason;
    memory_.denial_cooldown_until_s =
        ctx.snapshot.time_s + std::max(0.0, config_.denial_cooldown_s);
  }

  if (ctx.result.mode != MissionMode::Engage) {
    memory_.consecutive_engage_ticks = 0;
  }

  return finalize(ctx.result.mode);
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
