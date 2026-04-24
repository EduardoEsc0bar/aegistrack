"use client";

import { useEffect, useState } from "react";

import type { TelemetryUpdate } from "@/lib/types";
import { useUiStore } from "@/store/ui-store";

const WS_BASE_URL = process.env.NEXT_PUBLIC_WS_BASE_URL ?? "ws://localhost:8000";

export function useTelemetrySocket() {
  const [latestMessage, setLatestMessage] = useState<TelemetryUpdate | null>(null);
  const setConnectionState = useUiStore((state) => state.setConnectionState);

  useEffect(() => {
    const socket = new WebSocket(`${WS_BASE_URL}/ws/telemetry`);
    setConnectionState("connecting");

    socket.onopen = () => setConnectionState("connected");
    socket.onclose = () => setConnectionState("disconnected");
    socket.onerror = () => setConnectionState("disconnected");
    socket.onmessage = (event) => {
      try {
        setLatestMessage(JSON.parse(event.data) as TelemetryUpdate);
      } catch {
        setConnectionState("disconnected");
      }
    };

    return () => {
      socket.close();
    };
  }, [setConnectionState]);

  return latestMessage;
}
