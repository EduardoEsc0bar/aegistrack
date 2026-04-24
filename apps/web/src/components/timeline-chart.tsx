"use client";

import { Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";

import { Event } from "@/lib/types";

type Props = {
  events: Event[];
};

export function TimelineChart({ events }: Props) {
  const data = events.slice(0, 12).reverse().map((event, index) => ({
    label: event.occurred_at.slice(11, 19),
    value: index + (event.severity === "critical" ? 3 : event.severity === "warning" ? 2 : 1)
  }));

  return (
    <div className="h-64 rounded-2xl border border-line bg-panel/85 p-5">
      <div className="mb-4 flex items-center justify-between">
        <h2 className="text-lg font-semibold text-white">Mission Event Density</h2>
        <p className="text-xs uppercase tracking-[0.18em] text-muted">timeline</p>
      </div>
      <ResponsiveContainer width="100%" height="85%">
        <AreaChart data={data}>
          <defs>
            <linearGradient id="timelineFill" x1="0" y1="0" x2="0" y2="1">
              <stop offset="5%" stopColor="#4ad7d1" stopOpacity={0.6} />
              <stop offset="95%" stopColor="#4ad7d1" stopOpacity={0.05} />
            </linearGradient>
          </defs>
          <CartesianGrid stroke="#203245" strokeDasharray="3 3" />
          <XAxis dataKey="label" stroke="#8ea5bb" />
          <YAxis stroke="#8ea5bb" />
          <Tooltip />
          <Area dataKey="value" stroke="#4ad7d1" fill="url(#timelineFill)" />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}
