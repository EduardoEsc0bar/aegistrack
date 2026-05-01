import {
  AssignmentReplayEvent,
  BtDecisionReplayEvent,
  FaultReplayEvent,
  InterceptorStateReplayEvent,
  ReplayDataset,
  ReplayEvent,
  ReplayParseError,
  TrackReplayEvent,
  TrackStabilityReplayEvent,
  UnknownReplayEvent
} from "./types";

export function normalizeReplayEvents(
  events: ReplayEvent[],
  errors: ReplayParseError[] = []
): ReplayDataset {
  const finiteTimes = events
    .map((event) => event.t_s)
    .filter((time): time is number => typeof time === "number" && Number.isFinite(time));
  const startTimeS = finiteTimes.length > 0 ? Math.min(...finiteTimes) : 0;
  const endTimeS = finiteTimes.length > 0 ? Math.max(...finiteTimes) : 0;
  const eventsByType = groupEventsByType(events);

  return {
    events,
    errors,
    startTimeS,
    endTimeS,
    durationS: Math.max(0, endTimeS - startTimeS),
    eventsByType,
    tracks: typedEvents<TrackReplayEvent>(events, "track_event"),
    interceptors: typedEvents<InterceptorStateReplayEvent>(events, "interceptor_state"),
    assignments: typedEvents<AssignmentReplayEvent>(events, "assignment_event"),
    btDecisions: typedEvents<BtDecisionReplayEvent>(events, "bt_decision"),
    faults: typedEvents<FaultReplayEvent>(events, "fault_event"),
    trackStabilityEvents: typedEvents<TrackStabilityReplayEvent>(
      events,
      "track_stability_event"
    ),
    unknownEvents: typedEvents<UnknownReplayEvent>(events, "unknown")
  };
}

export function getEventsAtOrBefore(
  dataset: Pick<ReplayDataset, "events">,
  timeS: number
): ReplayEvent[] {
  return dataset.events.filter((event) => event.t_s !== null && event.t_s <= timeS);
}

export function getLatestEventByTypeAtOrBefore<T extends ReplayEvent>(
  dataset: Pick<ReplayDataset, "events">,
  type: T["type"],
  timeS: number
): T | null {
  const events = getEventsAtOrBefore(dataset, timeS).filter(
    (event): event is T => event.type === type
  );
  return events.at(-1) ?? null;
}

function groupEventsByType(events: ReplayEvent[]): Record<string, ReplayEvent[]> {
  return events.reduce<Record<string, ReplayEvent[]>>((groups, event) => {
    const key = event.type === "unknown" ? event.originalType : event.type;
    groups[key] = groups[key] ?? [];
    groups[key].push(event);
    return groups;
  }, {});
}

function typedEvents<T extends ReplayEvent>(events: ReplayEvent[], type: T["type"]): T[] {
  return events.filter((event): event is T => event.type === type);
}
