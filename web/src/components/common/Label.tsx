type Props = {
  children: string;
};

export function Label({ children }: Props) {
  return <p className="label">{children}</p>;
}
