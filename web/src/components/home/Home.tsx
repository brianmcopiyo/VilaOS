import { Row } from "../common/Row";

const ITEMS = [
  { id: "location", label: "Location" },
  { id: "navigate", label: "Navigate" },
  { id: "execute", label: "Execute" },
  { id: "help", label: "Help" },
] as const;

type Props = {
  focus: number;
  onSelect: (id: (typeof ITEMS)[number]["id"]) => void;
};

export function Home({ focus, onSelect }: Props) {
  return (
    <div className="stack">
      {ITEMS.map((item, index) => (
        <Row
          key={item.id}
          label={item.label}
          selected={index === focus}
          onClick={() => onSelect(item.id)}
        />
      ))}
    </div>
  );
}

export const homeCount = ITEMS.length;
export type HomeId = (typeof ITEMS)[number]["id"];
