export type Asset = {
  id: string;
  name: string;
  type: string;
  status: string;
  latitude: number;
  longitude: number;
  altitude: number;
  heading: number;
  speed: number;
  metadata: Record<string, unknown>;
  created_at: string;
  updated_at: string;
};

export type Sensor = {
  id: string;
  name: string;
  type: string;
  latitude: number;
  longitude: number;
  range_km: number;
  azimuth_deg: number;
  elevation_deg: number;
  is_active: boolean;
  metadata: Record<string, unknown>;
};

export type Track = {
  id: string;
  asset_id: string;
  classification: string;
  confidence: number;
  status: string;
  predicted_path: Array<{ lat: number; lon: number; alt: number }>;
  last_update_at: string;
};

export type Event = {
  id: string;
  asset_id: string | null;
  type: string;
  severity: "info" | "warning" | "critical";
  title: string;
  description: string;
  occurred_at: string;
  source: string;
  metadata: Record<string, unknown>;
};

export type PlaybackState = {
  id: string;
  mode: "live" | "playback";
  is_running: boolean;
  current_time: string | null;
  start_time: string | null;
  end_time: string | null;
  source: string;
  updated_at: string;
};

export type TelemetryUpdate = {
  type: "telemetry.update";
  timestamp: string;
  asset_id: string;
  position: { lat: number; lon: number; alt: number };
  velocity: { heading_deg: number; speed_mps: number };
  track: { confidence: number; classification: string };
  alerts: string[];
  source: string;
};

export type HealthResponse = {
  status: string;
  service: string;
  timestamp: string;
  profile: Record<string, unknown>;
  bridge: Record<string, unknown>;
};
