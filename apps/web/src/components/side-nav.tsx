"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";

import { classNames } from "@/lib/utils";

const items = [
  { href: "/dashboard", label: "Dashboard" },
  { href: "/globe", label: "Globe" },
  { href: "/timeline", label: "Timeline" },
  { href: "/assets", label: "Assets" },
  { href: "/settings", label: "Settings" }
];

export function SideNav() {
  const pathname = usePathname();

  return (
    <aside className="hidden w-64 border-r border-line bg-[#08121d]/90 px-5 py-6 lg:block">
      <p className="mb-8 text-sm font-semibold uppercase tracking-[0.32em] text-signal">AegisTrack</p>
      <nav className="space-y-2">
        {items.map((item) => (
          <Link
            key={item.href}
            href={item.href}
            className={classNames(
              "block rounded-xl border px-4 py-3 text-sm transition",
              pathname === item.href
                ? "border-signal/30 bg-signal/10 text-white"
                : "border-transparent bg-transparent text-muted hover:border-line hover:bg-white/5 hover:text-white"
            )}
          >
            {item.label}
          </Link>
        ))}
      </nav>
    </aside>
  );
}
