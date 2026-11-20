type Props = {
  label: string;
  selected?: boolean;
  onClick?: () => void;
};

export function Row({ label, selected = false, onClick }: Props) {
  return (
    <button
      type="button"
      className={`row${selected ? " row--on" : ""}`}
      onClick={onClick}
    >
      {label}
    </button>
  );
}
