import { Body } from "../common/Body";
import { Label } from "../common/Label";

export function Help() {
  return (
    <div className="stack">
      <Label>Controls</Label>
      <Body>Up / Down      Move in lists</Body>
      <Body>Left / Right   Move in actions</Body>
      <Body>Enter          Activate</Body>
      <Body>Esc            Back</Body>
      <Body>Type           Edit fields</Body>
      <Label>System</Label>
      <Body>Every screen shares the same shell.</Body>
      <Body>Actions always live in the bottom bar.</Body>
    </div>
  );
}
