import type { ComponentType, SVGProps } from "react";
import {
  CommandLineIcon,
  Cog6ToothIcon,
  ShoppingBagIcon,
} from "@heroicons/react/24/outline";

export type AppId = "terminal" | "settings" | "store";

export type AppDef = {
  id: AppId;
  name: string;
  blurb: string;
  icon: ComponentType<SVGProps<SVGSVGElement>>;
  installed: boolean;
};

export const APPS: AppDef[] = [
  {
    id: "terminal",
    name: "Terminal",
    blurb: "Low-level shell — pwd, cd, and run.",
    icon: CommandLineIcon,
    installed: true,
  },
  {
    id: "settings",
    name: "Settings",
    blurb: "System preferences and about.",
    icon: Cog6ToothIcon,
    installed: true,
  },
  {
    id: "store",
    name: "Store",
    blurb: "Browse and install desktop apps.",
    icon: ShoppingBagIcon,
    installed: true,
  },
];

export const STORE_CATALOG = APPS.map((a) => ({
  ...a,
  version: "1.0",
}));

export function appById(id: AppId): AppDef {
  return APPS.find((a) => a.id === id) ?? APPS[0];
}
