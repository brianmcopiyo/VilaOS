type Props = {
  children: string;
};

export function Hint({ children }: Props) {
  return <p className="hint">{children}</p>;
}
