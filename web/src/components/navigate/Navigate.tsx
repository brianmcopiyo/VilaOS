import { Field } from "../common/Field";
import { Hint } from "../common/Hint";
import { Label } from "../common/Label";

type Props = {
  path: string;
  onChange: (value: string) => void;
};

export function Navigate({ path, onChange }: Props) {
  return (
    <div className="stack">
      <Label>Destination</Label>
      <Field
        value={path}
        placeholder="Empty opens home"
        onChange={onChange}
      />
      <Hint>Type a path. Empty opens home.</Hint>
      <Hint>Then choose Go.</Hint>
    </div>
  );
}
