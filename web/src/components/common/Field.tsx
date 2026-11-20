type Props = {
  value: string;
  placeholder?: string;
  onChange: (value: string) => void;
};

export function Field({ value, placeholder, onChange }: Props) {
  return (
    <input
      className="field"
      value={value}
      placeholder={placeholder}
      onChange={(e) => onChange(e.target.value)}
      spellCheck={false}
      autoComplete="off"
    />
  );
}
