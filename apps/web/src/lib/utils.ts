export function formatUtc(timestamp: string | null): string {
  if (!timestamp) return "Unavailable";
  return new Date(timestamp).toLocaleString("en-US", {
    dateStyle: "medium",
    timeStyle: "medium",
    timeZone: "UTC"
  });
}

export function severityTone(severity: string): "danger" | "warning" | "neutral" {
  if (severity === "critical") return "danger";
  if (severity === "warning") return "warning";
  return "neutral";
}

export function classNames(...values: Array<string | false | null | undefined>): string {
  return values.filter(Boolean).join(" ");
}
