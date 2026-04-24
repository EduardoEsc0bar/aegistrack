"use client";

import { StatusBadge } from "@/components/status-badge";
import { useUiStore } from "@/store/ui-store";

export function ConnectionIndicator() {
  const connectionState = useUiStore((state) => state.connectionState);

  if (connectionState === "connected") {
    return <StatusBadge label="WebSocket Linked" tone="success" />;
  }
  if (connectionState === "connecting") {
    return <StatusBadge label="Link Pending" tone="warning" />;
  }
  return <StatusBadge label="Link Down" tone="danger" />;
}
