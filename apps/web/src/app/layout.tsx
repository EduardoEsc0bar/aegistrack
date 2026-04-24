import type { Metadata } from "next";

import { Providers } from "@/lib/providers";

import "cesium/Build/Cesium/Widgets/widgets.css";
import "./globals.css";

export const metadata: Metadata = {
  title: "AegisTrack Mission Control",
  description: "Mission-operations web layer integrated into the existing AegisTrack tracking and replay system."
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>
        <Providers>{children}</Providers>
      </body>
    </html>
  );
}
