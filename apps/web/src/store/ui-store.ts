"use client";

import { create } from "zustand";

type LayerState = {
  tracks: boolean;
  sensors: boolean;
  predictedPaths: boolean;
  coverageCones: boolean;
  alerts: boolean;
  heatmap: boolean;
};

type UiState = {
  selectedEntityId: string | null;
  playbackMode: "live" | "playback";
  connectionState: "connected" | "disconnected" | "connecting";
  sidePanelOpen: boolean;
  layers: LayerState;
  setSelectedEntityId: (entityId: string | null) => void;
  setPlaybackMode: (mode: "live" | "playback") => void;
  setConnectionState: (state: UiState["connectionState"]) => void;
  toggleSidePanel: (open?: boolean) => void;
  toggleLayer: (key: keyof LayerState) => void;
};

export const useUiStore = create<UiState>((set) => ({
  selectedEntityId: null,
  playbackMode: "live",
  connectionState: "connecting",
  sidePanelOpen: true,
  layers: {
    tracks: true,
    sensors: true,
    predictedPaths: true,
    coverageCones: false,
    alerts: true,
    heatmap: false
  },
  setSelectedEntityId: (selectedEntityId) => set({ selectedEntityId }),
  setPlaybackMode: (playbackMode) => set({ playbackMode }),
  setConnectionState: (connectionState) => set({ connectionState }),
  toggleSidePanel: (open) =>
    set((state) => ({ sidePanelOpen: typeof open === "boolean" ? open : !state.sidePanelOpen })),
  toggleLayer: (key) =>
    set((state) => ({ layers: { ...state.layers, [key]: !state.layers[key] } }))
}));
