"use client";

import { create } from "zustand";

import { parseReplayJsonl } from "../lib/replay/parser";
import { ReplayDataset } from "../lib/replay/types";

type ReplayStore = {
  dataset: ReplayDataset | null;
  currentTimeS: number;
  isPlaying: boolean;
  playbackSpeed: number;
  selectedEventId: string | null;
  loadReplayFromText: (text: string) => ReplayDataset;
  setCurrentTimeS: (timeS: number) => void;
  play: () => void;
  pause: () => void;
  setPlaybackSpeed: (speed: number) => void;
  setSelectedEventId: (eventId: string | null) => void;
  clearReplay: () => void;
};

export const useReplayStore = create<ReplayStore>((set, get) => ({
  dataset: null,
  currentTimeS: 0,
  isPlaying: false,
  playbackSpeed: 1,
  selectedEventId: null,
  loadReplayFromText: (text) => {
    const dataset = parseReplayJsonl(text);
    set({
      dataset,
      currentTimeS: dataset.startTimeS,
      isPlaying: false,
      selectedEventId: null
    });
    return dataset;
  },
  setCurrentTimeS: (timeS) => {
    const dataset = get().dataset;
    if (!dataset) {
      set({ currentTimeS: Math.max(0, timeS) });
      return;
    }
    set({
      currentTimeS: Math.min(dataset.endTimeS, Math.max(dataset.startTimeS, timeS))
    });
  },
  play: () => set({ isPlaying: true }),
  pause: () => set({ isPlaying: false }),
  setPlaybackSpeed: (playbackSpeed) =>
    set({ playbackSpeed: Math.max(0.1, Math.min(16, playbackSpeed)) }),
  setSelectedEventId: (selectedEventId) => set({ selectedEventId }),
  clearReplay: () =>
    set({
      dataset: null,
      currentTimeS: 0,
      isPlaying: false,
      selectedEventId: null
    })
}));
