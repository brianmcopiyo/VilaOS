type Props = {
  children: string;
};

export function Body({ children }: Props) {
  return <p className="body">{children}</p>;
}
