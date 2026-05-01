import {
  AssignmentReplayEvent,
  BtDecisionReplayEvent,
  BtEventTraceReplayEvent,
  BtNodeTraceReplayEvent,
  CommandReplayEvent,
  DemoMetadataReplayEvent,
  DecisionReplayEvent,
  FaultReplayEvent,
  InterceptReplayEvent,
  InterceptorStateReplayEvent,
  JsonObject,
  JsonValue,
  MeasurementReplayEvent,
  ReplayDataset,
  ReplayEvent,
  ReplayEventMeta,
  ReplayKnownEventType,
  ReplayParseError,
  ReplayParseResult,
  TrackReplayEvent,
  TrackStabilityReplayEvent,
  UnknownReplayEvent
} from "./types";
import { normalizeReplayEvents } from "./normalizers";

const knownEventTypes: ReadonlySet<string> = new Set<ReplayKnownEventType>([
  "measurement",
  "track_event",
  "track_stability_event",
  "decision_event",
  "command_event",
  "assignment_event",
  "intercept_event",
  "interceptor_state",
  "bt_decision",
  "fault_event",
  "demo_metadata"
]);

export function parseReplayJsonl(text: string): ReplayDataset {
  const result = parseReplayEvents(text);
  return normalizeReplayEvents(result.events, result.errors);
}

export function parseReplayEvents(text: string): ReplayParseResult {
  const events: ReplayEvent[] = [];
  const errors: ReplayParseError[] = [];

  text.split(/\r?\n/).forEach((line, index) => {
    const lineNumber = index + 1;
    const trimmed = line.trim();
    if (!trimmed) {
      return;
    }

    try {
      const parsed = JSON.parse(trimmed) as unknown;
      if (!isJsonObject(parsed)) {
        errors.push({ lineNumber, message: "line is not a JSON object", line });
        return;
      }
      events.push(toReplayEvent(parsed, lineNumber, events.length));
    } catch (error) {
      errors.push({
        lineNumber,
        message: error instanceof Error ? error.message : "invalid JSON",
        line
      });
    }
  });

  events.sort((a, b) => {
    const aTime = a.t_s ?? Number.POSITIVE_INFINITY;
    const bTime = b.t_s ?? Number.POSITIVE_INFINITY;
    if (aTime !== bTime) {
      return aTime - bTime;
    }
    return a.lineNumber - b.lineNumber;
  });

  return { events, errors };
}

function toReplayEvent(raw: JsonObject, lineNumber: number, originalIndex: number): ReplayEvent {
  const originalType = asString(raw.type) ?? "unknown";
  const type = knownEventTypes.has(originalType) ? originalType : "unknown";
  const t_s = deriveEventTime(raw);
  const meta: ReplayEventMeta = {
    eventId: `${lineNumber}:${originalType}:${asNumber(raw.trace_id) ?? originalIndex}`,
    lineNumber,
    originalIndex,
    t_s,
    raw
  };

  switch (type) {
    case "measurement":
      return {
        ...meta,
        type,
        t_meas_s: asNumber(raw.t_meas_s),
        t_sent_s: asNumber(raw.t_sent_s),
        sensor_id: asNumber(raw.sensor_id),
        sensor_type: asString(raw.sensor_type),
        measurement_type: asString(raw.measurement_type),
        z_dim: asNumber(raw.z_dim),
        z: asNumberArray(raw.z),
        R: asNumberArray(raw.R),
        confidence: asNumber(raw.confidence)
      } satisfies MeasurementReplayEvent;
    case "track_event":
      return {
        ...meta,
        type,
        event: asString(raw.event),
        track_id: asNumber(raw.track_id),
        status: asString(raw.status),
        x: asNumberArray(raw.x),
        P: asNumberArray(raw.P),
        age_ticks: asNumber(raw.age_ticks),
        hits: asNumber(raw.hits),
        misses: asNumber(raw.misses),
        score: asNumber(raw.score),
        confidence: asNumber(raw.confidence)
      } satisfies TrackReplayEvent;
    case "track_stability_event":
      return {
        ...meta,
        type,
        event: asString(raw.event),
        track_id: asNumber(raw.track_id),
        status: asString(raw.status),
        age_ticks: asNumber(raw.age_ticks),
        hits: asNumber(raw.hits),
        misses: asNumber(raw.misses),
        score: asNumber(raw.score),
        confidence: asNumber(raw.confidence),
        reason: asString(raw.reason),
        related_track_id: asNumber(raw.related_track_id),
        distance_m: asNumber(raw.distance_m)
      } satisfies TrackStabilityReplayEvent;
    case "decision_event":
      return {
        ...meta,
        type,
        track_id: asNumber(raw.track_id),
        decision_type: asString(raw.decision_type),
        reason: asString(raw.reason)
      } satisfies DecisionReplayEvent;
    case "command_event":
      return {
        ...meta,
        type,
        track_id: asNumber(raw.track_id),
        command_type: asString(raw.command_type),
        detail: asString(raw.detail)
      } satisfies CommandReplayEvent;
    case "assignment_event":
      return {
        ...meta,
        type,
        interceptor_id: asNumber(raw.interceptor_id),
        track_id: asNumber(raw.track_id),
        decision_type: asString(raw.decision_type),
        reason: asString(raw.reason)
      } satisfies AssignmentReplayEvent;
    case "intercept_event":
      return {
        ...meta,
        type,
        track_id: asNumber(raw.track_id),
        outcome: asString(raw.outcome),
        reason: asString(raw.reason)
      } satisfies InterceptReplayEvent;
    case "interceptor_state":
      return {
        ...meta,
        type,
        interceptor_id: asNumber(raw.interceptor_id),
        engaged: Boolean(asNumber(raw.engaged) ?? raw.engaged),
        track_id: asNumber(raw.track_id),
        position: asNumberArray(raw.position),
        velocity: asNumberArray(raw.velocity),
        engagement_time_s: asNumber(raw.engagement_time_s)
      } satisfies InterceptorStateReplayEvent;
    case "bt_decision":
      return {
        ...meta,
        type,
        tick: asNumber(raw.tick),
        mode: asString(raw.mode),
        track_id: asNumber(raw.track_id),
        decision_type: asString(raw.decision_type),
        reason: asString(raw.reason),
        engagement_commands: asNumber(raw.engagement_commands),
        active_engagements: asNumber(raw.active_engagements),
        idle_interceptors: asNumber(raw.idle_interceptors),
        reconciliation_removals: asNumber(raw.reconciliation_removals),
        active_engagements_after_reconcile: asNumber(
          raw.active_engagements_after_reconcile
        ),
        selected_track_id: asNumber(raw.selected_track_id),
        selected_interceptor_id: asNumber(raw.selected_interceptor_id),
        selected_engagement_score: asNumber(raw.selected_engagement_score),
        selected_estimated_intercept_time_s: asNumber(
          raw.selected_estimated_intercept_time_s
        ),
        assignment_reason: asString(raw.assignment_reason),
        events: asBtEvents(raw.events),
        nodes: asBtNodes(raw.nodes)
      } satisfies BtDecisionReplayEvent;
    case "fault_event":
      return {
        ...meta,
        type,
        fault: asString(raw.fault)
      } satisfies FaultReplayEvent;
    case "demo_metadata":
      return {
        ...meta,
        type,
        mission_name: asString(raw.mission_name),
        duration_s: asNumber(raw.duration_s),
        track_count: asNumber(raw.track_count),
        interceptor_count: asNumber(raw.interceptor_count),
        fault_profile: asString(raw.fault_profile)
      } satisfies DemoMetadataReplayEvent;
    default:
      return {
        ...meta,
        type: "unknown",
        originalType
      } satisfies UnknownReplayEvent;
  }
}

function deriveEventTime(raw: JsonObject): number | null {
  return asNumber(raw.t_s) ?? asNumber(raw.t_sent_s) ?? asNumber(raw.t_meas_s);
}

function isJsonObject(value: unknown): value is JsonObject {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function asString(value: JsonValue | undefined): string | null {
  return typeof value === "string" ? value : null;
}

function asNumber(value: JsonValue | undefined): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function asNumberArray(value: JsonValue | undefined): number[] {
  if (!Array.isArray(value)) {
    return [];
  }
  return value.filter((item): item is number => typeof item === "number" && Number.isFinite(item));
}

function asBtEvents(value: JsonValue | undefined): BtEventTraceReplayEvent[] {
  if (!Array.isArray(value)) {
    return [];
  }
  return value.filter(isJsonObject).map((item) => ({
    event: asString(item.event) ?? "unknown",
    track_id: asNumber(item.track_id),
    interceptor_id: asNumber(item.interceptor_id),
    reason: asString(item.reason) ?? ""
  }));
}

function asBtNodes(value: JsonValue | undefined): BtNodeTraceReplayEvent[] {
  if (!Array.isArray(value)) {
    return [];
  }
  return value.filter(isJsonObject).map((item) => ({
    node: asString(item.node) ?? "unknown",
    status: asString(item.status) ?? "unknown",
    detail: asString(item.detail) ?? ""
  }));
}
