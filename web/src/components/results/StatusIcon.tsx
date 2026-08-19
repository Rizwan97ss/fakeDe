import { CheckCircle2, AlertTriangle, XOctagon } from "lucide-react";
import type { StatusTone } from "../../lib/status";
import { STATUS_COLOR } from "../../lib/status";

const ICONS: Record<StatusTone, typeof CheckCircle2> = {
  good: CheckCircle2,
  warning: AlertTriangle,
  critical: XOctagon,
};

export function StatusIcon({ tone, size = 20 }: { tone: StatusTone; size?: number }) {
  const Icon = ICONS[tone];
  return <Icon size={size} color={STATUS_COLOR[tone]} strokeWidth={2.25} aria-hidden="true" />;
}
