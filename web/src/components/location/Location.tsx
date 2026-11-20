import { Hint } from "../common/Hint";
import { Label } from "../common/Label";
import { Value } from "../common/Value";

type Props = {
  path: string;
};

export function Location({ path }: Props) {
  return (
    <div className="stack">
      <Label>Current location</Label>
      <Value>{path}</Value>
      <Hint>Use Refresh to update.</Hint>
    </div>
  );
}
