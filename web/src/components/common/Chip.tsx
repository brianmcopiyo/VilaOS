type Props = {
  label: string;
  selected?: boolean;
  onClick?: () => void;
};

export function Chip({ label, selected = false, onClick }: Props) {
  return (
    <button
      type="button"
      className={`chip${selected ? " chip--on" : ""}`}
      onClick={onClick}
    >
      {label}
    </button>
  );
}
