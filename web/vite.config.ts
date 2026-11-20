import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

// Shared UI. Target picks which build folder receives the bundle.
const outDirs: Record<string, string> = {
  win: "../winbuild/bin/ui",
  iso: "../kernelbuild/iso_root/ui",
};

const target = process.env.UI_TARGET || "win";
const outDir = outDirs[target] ?? outDirs.win;

export default defineConfig({
  plugins: [react()],
  base: "./",
  build: {
    outDir,
    emptyOutDir: true,
  },
});
