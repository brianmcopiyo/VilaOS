type Props = {
  children: string;
};

export function Value({ children }: Props) {
  return <div className="value">{children}</div>;
}
