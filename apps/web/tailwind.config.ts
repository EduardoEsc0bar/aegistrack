import type { Config } from "tailwindcss";

const config: Config = {
  content: ["./src/**/*.{js,ts,jsx,tsx,mdx}"],
  theme: {
    extend: {
      colors: {
        ink: "#07111d",
        steel: "#0f1c2a",
        panel: "#132234",
        line: "#243851",
        signal: "#4ad7d1",
        warning: "#ffb65c",
        danger: "#ff6b6b",
        text: "#dbe8f5",
        muted: "#8ea5bb"
      },
      boxShadow: {
        panel: "0 12px 40px rgba(3, 10, 20, 0.4)"
      },
      fontFamily: {
        sans: ["ui-sans-serif", "system-ui", "sans-serif"],
        mono: ["ui-monospace", "SFMono-Regular", "monospace"]
      }
    }
  },
  plugins: []
};

export default config;
