export type JsonValue =
  | string
  | number
  | boolean
  | null
  | JsonValue[]
  | { [key: string]: JsonValue };

export type JsonObject = { [key: string]: JsonValue };

export type ReplayKnownEventType =
  | "measurement"
  | "track_event"
  | "track_stability_event"
  | "decision_event"
  | "command_event"
  | "assignment_event"
  | "intercept_event"
  | "interceptor_state"
  | "bt_decision"
  | "fault_event"
  | "demo_metadata";

export type ReplayEventType = ReplayKnownEventType | "unknown";

export type ReplayEventMeta = {
  eventId: string;
  lineNumber: number;
  originalIndex: number;
  t_s: number | null;
  raw: JsonObject;
};

export type MeasurementReplayEvent = ReplayEventMeta & {
  type: "measurement";
  t_meas_s: number | null;
  t_sent_s: number | null;
  sensor_id: number | null;
  sensor_type: string | null;
  measurement_type: string | null;
  z_dim: number | null;
  z: number[];
  R: number[];
  confidence: number | null;
};

export type TrackReplayEvent = ReplayEventMeta & {
  type: "track_event";
  event: string | null;
  track_id: number | null;
  status: string | null;
  x: number[];
  P: number[];
  age_ticks: number | null;
  hits: number | null;
  misses: number | null;
  score: number | null;
  confidence: number | null;
};

export type TrackStabilityReplayEvent = ReplayEventMeta & {
  type: "track_stability_event";
  event: string | null;
  track_id: number | null;
  status: string | null;
  age_ticks: number | null;
  hits: number | null;
  misses: number | null;
  score: number | null;
  confidence: number | null;
  reason: string | null;
  related_track_id: number | null;
  distance_m: number | null;
};

export type DecisionReplayEvent = ReplayEventMeta & {
  type: "decision_event";
  track_id: number | null;
  decision_type: string | null;
  reason: string | null;
};

export type CommandReplayEvent = ReplayEventMeta & {
  type: "command_event";
  track_id: number | null;
  command_type: string | null;
  detail: string | null;
};

export type AssignmentReplayEvent = ReplayEventMeta & {
  type: "assignment_event";
  interceptor_id: number | null;
  track_id: number | null;
  decision_type: string | null;
  reason: string | null;
};

export type InterceptReplayEvent = ReplayEventMeta & {
  type: "intercept_event";
  track_id: number | null;
  outcome: string | null;
  reason: string | null;
};

export type InterceptorStateReplayEvent = ReplayEventMeta & {
  type: "interceptor_state";
  interceptor_id: number | null;
  engaged: boolean;
  track_id: number | null;
  position: number[];
  velocity: number[];
  engagement_time_s: number | null;
};

export type BtNodeTraceReplayEvent = {
  node: string;
  status: string;
  detail: string;
};

export type BtEventTraceReplayEvent = {
  event: string;
  track_id: number | null;
  interceptor_id: number | null;
  reason: string;
};

export type BtDecisionReplayEvent = ReplayEventMeta & {
  type: "bt_decision";
  tick: number | null;
  mode: string | null;
  track_id: number | null;
  decision_type: string | null;
  reason: string | null;
  engagement_commands: number | null;
  active_engagements: number | null;
  idle_interceptors: number | null;
  reconciliation_removals: number | null;
  active_engagements_after_reconcile: number | null;
  selected_track_id: number | null;
  selected_interceptor_id: number | null;
  selected_engagement_score: number | null;
  selected_estimated_intercept_time_s: number | null;
  assignment_reason: string | null;
  events: BtEventTraceReplayEvent[];
  nodes: BtNodeTraceReplayEvent[];
};

export type FaultReplayEvent = ReplayEventMeta & {
  type: "fault_event";
  fault: string | null;
};

export type DemoMetadataReplayEvent = ReplayEventMeta & {
  type: "demo_metadata";
  mission_name: string | null;
  duration_s: number | null;
  track_count: number | null;
  interceptor_count: number | null;
  fault_profile: string | null;
};

export type UnknownReplayEvent = ReplayEventMeta & {
  type: "unknown";
  originalType: string;
};

export type ReplayEvent =
  | MeasurementReplayEvent
  | TrackReplayEvent
  | TrackStabilityReplayEvent
  | DecisionReplayEvent
  | CommandReplayEvent
  | AssignmentReplayEvent
  | InterceptReplayEvent
  | InterceptorStateReplayEvent
  | BtDecisionReplayEvent
  | FaultReplayEvent
  | DemoMetadataReplayEvent
  | UnknownReplayEvent;

export type ReplayParseError = {
  lineNumber: number;
  message: string;
  line: string;
};

export type ReplayParseResult = {
  events: ReplayEvent[];
  errors: ReplayParseError[];
};

export type ReplayDataset = ReplayParseResult & {
  startTimeS: number;
  endTimeS: number;
  durationS: number;
  eventsByType: Record<string, ReplayEvent[]>;
  tracks: TrackReplayEvent[];
  interceptors: InterceptorStateReplayEvent[];
  assignments: AssignmentReplayEvent[];
  btDecisions: BtDecisionReplayEvent[];
  faults: FaultReplayEvent[];
  trackStabilityEvents: TrackStabilityReplayEvent[];
  unknownEvents: UnknownReplayEvent[];
};
