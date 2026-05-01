import { beforeEach, describe, expect, it } from "vitest";

import { useReplayStore } from "../../store/replay-store";
import { sampleMissionJsonl } from "./fixtures/sample-mission";
import { getEventsAtOrBefore } from "./normalizers";
import { parseReplayEvents, parseReplayJsonl } from "./parser";

describe("replay JSONL parser", () => {
  beforeEach(() => {
    useReplayStore.getState().clearReplay();
  });

  it("skips blank lines and sorts events by replay time", () => {
    const dataset = parseReplayJsonl(`

{"type":"fault_event","t_s":2.0,"fault":"late"}
{"type":"fault_event","t_s":1.0,"fault":"early"}

`);

    expect(dataset.events).toHaveLength(2);
    expect(dataset.events[0]?.t_s).toBe(1.0);
    expect(dataset.events[1]?.t_s).toBe(2.0);
  });

  it("preserves unknown events", () => {
    const dataset = parseReplayJsonl('{"type":"new_runtime_event","t_s":1.5,"value":42}');

    expect(dataset.unknownEvents).toHaveLength(1);
    expect(dataset.unknownEvents[0]?.originalType).toBe("new_runtime_event");
    expect(dataset.eventsByType.new_runtime_event).toHaveLength(1);
  });

  it("records malformed line errors without dropping valid events", () => {
    const result = parseReplayEvents(`
{"type":"fault_event","t_s":1.0,"fault":"drop"}
not-json
{"type":"fault_event","t_s":2.0,"fault":"jitter"}
`);

    expect(result.events).toHaveLength(2);
    expect(result.errors).toHaveLength(1);
    expect(result.errors[0]?.lineNumber).toBe(3);
  });

  it("normalizes sample duration and groups BT decisions", () => {
    const dataset = parseReplayJsonl(sampleMissionJsonl);

    expect(dataset.startTimeS).toBe(0.0);
    expect(dataset.endTimeS).toBe(8.0);
    expect(dataset.durationS).toBeCloseTo(8.0);
    expect(dataset.eventsByType.demo_metadata).toHaveLength(1);
    expect(dataset.btDecisions).toHaveLength(10);
    expect(dataset.assignments).toHaveLength(5);
    expect(dataset.trackStabilityEvents.length).toBeGreaterThanOrEqual(10);
    expect(getEventsAtOrBefore(dataset, 0.0).length).toBeGreaterThan(10);
  });

  it("loads sample text into the replay store", () => {
    const dataset = useReplayStore.getState().loadReplayFromText(sampleMissionJsonl);
    const state = useReplayStore.getState();

    expect(dataset.events.length).toBeGreaterThan(0);
    expect(state.dataset?.btDecisions).toHaveLength(10);
    expect(state.currentTimeS).toBe(dataset.startTimeS);
    expect(state.isPlaying).toBe(false);
  });
});
