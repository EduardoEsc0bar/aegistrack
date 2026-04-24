"use client";

import { useMemo, useState } from "react";

import { AppShell } from "@/components/app-shell";
import { EntityDetailsPanel } from "@/components/entity-details-panel";
import { StatusBadge } from "@/components/status-badge";
import { useMissionControlData } from "@/hooks/use-mission-control-data";
import { useUiStore } from "@/store/ui-store";

export function AssetsView() {
  const { assets, tracks, sensors } = useMissionControlData();
  const [query, setQuery] = useState("");
  const setSelectedEntityId = useUiStore((state) => state.setSelectedEntityId);

  const filtered = useMemo(
    () =>
      (assets.data ?? []).filter((asset) =>
        `${asset.name} ${asset.type} ${asset.status}`.toLowerCase().includes(query.toLowerCase())
      ),
    [assets.data, query]
  );

  return (
    <AppShell
      title="Assets And Tracks"
      subtitle="Searchable operator inventory of tracked entities, interceptors, and sensor-linked mission objects."
    >
      <section className="grid gap-6 xl:grid-cols-[1fr_320px]">
        <div className="rounded-2xl border border-line bg-panel/85 p-5">
          <div className="mb-4 flex flex-col gap-3 lg:flex-row lg:items-center lg:justify-between">
            <h2 className="text-lg font-semibold text-white">Tracked Assets</h2>
            <input
              value={query}
              onChange={(event) => setQuery(event.target.value)}
              placeholder="Search asset, type, or status"
              className="rounded-xl border border-line bg-ink/60 px-4 py-2 text-sm text-white outline-none"
            />
          </div>
          <div className="overflow-hidden rounded-xl border border-white/5">
            <table className="min-w-full divide-y divide-white/5 text-left text-sm">
              <thead className="bg-ink/70 text-muted">
                <tr>
                  <th className="px-4 py-3">Name</th>
                  <th className="px-4 py-3">Type</th>
                  <th className="px-4 py-3">Status</th>
                  <th className="px-4 py-3">Location</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-white/5">
                {filtered.map((asset) => (
                  <tr
                    key={asset.id}
                    className="cursor-pointer bg-panel/40 hover:bg-panel/70"
                    onClick={() => setSelectedEntityId(asset.id)}
                  >
                    <td className="px-4 py-3 text-white">{asset.name}</td>
                    <td className="px-4 py-3 text-muted">{asset.type}</td>
                    <td className="px-4 py-3">
                      <StatusBadge label={asset.status} tone={asset.status === "active" ? "success" : "neutral"} />
                    </td>
                    <td className="px-4 py-3 text-muted">
                      {asset.latitude.toFixed(2)}, {asset.longitude.toFixed(2)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <div className="mt-4 grid gap-4 lg:grid-cols-3 text-sm text-muted">
            <div>Sensors online: {sensors.data?.length ?? 0}</div>
            <div>Tracks visible: {tracks.data?.length ?? 0}</div>
            <div>Asset rows: {filtered.length}</div>
          </div>
        </div>
        <EntityDetailsPanel assets={assets.data ?? []} tracks={tracks.data ?? []} />
      </section>
    </AppShell>
  );
}
