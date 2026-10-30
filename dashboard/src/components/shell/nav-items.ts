import {
  ArrowLeftRight,
  Clock,
  Cpu,
  HardDrive,
  Hash,
  LayoutDashboard,
  List,
  ListOrdered,
  Radio,
  Repeat,
  Shapes,
  SquareTerminal,
  Type,
  type LucideIcon,
} from "lucide-react";

export interface NavItem {
  href: string;
  label: string;
  icon: LucideIcon;
  group: "Observe" | "Data types" | "Semantics" | "Systems";
}

export const NAV_ITEMS: NavItem[] = [
  { href: "/", label: "Overview", icon: LayoutDashboard, group: "Observe" },
  { href: "/cli", label: "Raw CLI", icon: SquareTerminal, group: "Observe" },
  { href: "/strings", label: "Strings", icon: Type, group: "Data types" },
  { href: "/hashes", label: "Hashes", icon: Hash, group: "Data types" },
  { href: "/lists", label: "Lists", icon: List, group: "Data types" },
  { href: "/sets", label: "Sets", icon: Shapes, group: "Data types" },
  { href: "/sorted-sets", label: "Sorted Sets", icon: ListOrdered, group: "Data types" },
  { href: "/ttl", label: "TTL", icon: Clock, group: "Semantics" },
  { href: "/transactions", label: "Transactions", icon: Repeat, group: "Semantics" },
  { href: "/pubsub", label: "Pub/Sub", icon: Radio, group: "Semantics" },
  { href: "/memory", label: "Memory / LRU", icon: Cpu, group: "Systems" },
  { href: "/replication", label: "Replication", icon: ArrowLeftRight, group: "Systems" },
  { href: "/persistence", label: "Persistence", icon: HardDrive, group: "Systems" },
];

export const NAV_GROUPS = ["Observe", "Data types", "Semantics", "Systems"] as const;

