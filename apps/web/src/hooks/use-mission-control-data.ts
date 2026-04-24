"use client";

import { useQuery } from "@tanstack/react-query";

import { api } from "@/lib/api";

export function useMissionControlData() {
  const health = useQuery({ queryKey: ["health"], queryFn: api.getHealth });
  const assets = useQuery({ queryKey: ["assets"], queryFn: api.getAssets });
  const sensors = useQuery({ queryKey: ["sensors"], queryFn: api.getSensors });
  const tracks = useQuery({ queryKey: ["tracks"], queryFn: api.getTracks });
  const events = useQuery({ queryKey: ["events"], queryFn: api.getEvents });
  const telemetry = useQuery({ queryKey: ["telemetry"], queryFn: api.getTelemetry, refetchInterval: 3000 });
  const playback = useQuery({ queryKey: ["playback"], queryFn: api.getPlaybackState, refetchInterval: 3000 });

  return { health, assets, sensors, tracks, events, telemetry, playback };
}
