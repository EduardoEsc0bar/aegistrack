import {
  Asset,
  Event,
  HealthResponse,
  PlaybackState,
  Sensor,
  TelemetryUpdate,
  Track
} from "@/lib/types";

const API_BASE_URL = process.env.NEXT_PUBLIC_API_BASE_URL ?? "http://localhost:8000";

async function fetchJson<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(`${API_BASE_URL}${path}`, {
    cache: "no-store",
    ...init
  });
  if (!response.ok) {
    throw new Error(`Request failed: ${response.status}`);
  }
  return response.json() as Promise<T>;
}

export const api = {
  getHealth: () => fetchJson<HealthResponse>("/health"),
  getAssets: () => fetchJson<Asset[]>("/api/v1/assets"),
  getSensors: () => fetchJson<Sensor[]>("/api/v1/sensors"),
  getTracks: () => fetchJson<Track[]>("/api/v1/tracks"),
  getEvents: () => fetchJson<Event[]>("/api/v1/events"),
  getTelemetry: () => fetchJson<TelemetryUpdate[]>("/api/v1/telemetry/latest"),
  getPlaybackState: () => fetchJson<PlaybackState>("/api/v1/playback/state"),
  startPlayback: async () =>
    fetchJson<{ state: PlaybackState }>("/api/v1/playback/start", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: "{}"
    }),
  stopPlayback: async () =>
    fetchJson<{ state: PlaybackState }>("/api/v1/playback/stop", { method: "POST" })
};
