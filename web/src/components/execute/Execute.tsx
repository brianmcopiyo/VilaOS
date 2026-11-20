import { Field } from "../common/Field";
import { Label } from "../common/Label";
import { Value } from "../common/Value";

type Props = {
  command: string;
  result: string;
  onChange: (value: string) => void;
};

export function Execute({ command, result, onChange }: Props) {
  return (
    <div className="stack">
      <Label>Command</Label>
      <Field
        value={command}
        placeholder="cmd /c dir"
        onChange={onChange}
      />
      <Label>Result</Label>
      <Value>{result}</Value>
    </div>
  );
}
