import { describe, expect, it } from "vitest";

import { sampleMissionJsonl } from "./fixtures/sample-mission";
import { parseReplayJsonl } from "./parser";
import {
  getActiveAssignmentsAt,
  getActiveInterceptorsAt,
  getActiveTracksAt,
  getLatestBtDecisionAt,
  getRecentEventsAt,
  getReplayFrameAt
} from "./selectors";

describe("replay selectors", () => {
  const dataset = parseReplayJsonl(sampleMissionJsonl);

  it("builds a frame with latest track and interceptor before a time", () => {
    const frame = getReplayFrameAt(dataset, 0.0);

    expect(frame.tracks).toHaveLength(5);
    expect(frame.tracks[0]?.trackId).toBe(101);
    expect(frame.tracks[0]?.x).toBe(1380);
    expect(frame.interceptors).toHaveLength(3);
    expect(frame.interceptors[0]?.engaged).toBe(true);
    expect(frame.interceptors[0]?.x).toBe(-700);
    expect(frame.assignments).toHaveLength(2);
  });

  it("selects the latest BT decision at or before the playhead", () => {
    const decision = getLatestBtDecisionAt(dataset, 1.7);

    expect(decision?.mode).toBe("engage");
    expect(decision?.selected_interceptor_id).toBe(3);
  });

  it("sorts recent mission events newest first", () => {
    const events = getRecentEventsAt(dataset, 0.2, 3);

    expect(events).toHaveLength(3);
    expect(events[0]?.t_s).toBeGreaterThanOrEqual(events[1]?.t_s ?? 0);
  });

  it("includes active assignments until an intercept occurs", () => {
    expect(getActiveAssignmentsAt(dataset, 2.0)).toHaveLength(3);
    expect(getActiveAssignmentsAt(dataset, 3.8).some((item) => item.trackId === 101)).toBe(false);
    expect(getActiveAssignmentsAt(dataset, 4.3).some((item) => item.trackId === 105)).toBe(true);
  });

  it("exposes direct active track and interceptor helpers", () => {
    expect(getActiveTracksAt(dataset, 0.0)[0]?.trackId).toBe(101);
    expect(getActiveInterceptorsAt(dataset, 0.0)[0]?.interceptorId).toBe(1);
  });
});
