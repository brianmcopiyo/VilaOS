(function (g) {
  function h(type, props) {
    var children = Array.prototype.slice.call(arguments, 2);
    var flat = [];
    function push(c) {
      if (c == null || c === false) return;
      if (Array.isArray(c)) c.forEach(push);
      else flat.push(c);
    }
    children.forEach(push);
    if (typeof type === "function") {
      var p = Object.assign({}, props || {}, { children: flat });
      return type(p);
    }
    return { type: type, props: props || {}, children: flat };
  }

  function applyStyle(el, props) {
    var s = el.style;
    if (props.background != null) s.background = props.background;
    if (props.color != null) s.color = props.color;
    if (props.borderColor != null) {
      s.borderStyle = "solid";
      s.borderColor = props.borderColor;
    }
    if (props.borderWidth != null) {
      s.borderStyle = "solid";
      s.borderWidth = String(props.borderWidth).indexOf("px") >= 0 ? props.borderWidth : props.borderWidth + "px";
    }
    if (props.borderRadius != null) s.borderRadius = String(props.borderRadius).indexOf("px") >= 0 ? props.borderRadius : props.borderRadius + "px";
    if (props.padding != null) s.padding = props.padding;
    if (props.margin != null) s.margin = props.margin;
    if (props.gap != null) s.gap = props.gap;
    if (props.columnGap != null) s.columnGap = props.columnGap;
    if (props.rowGap != null) s.rowGap = props.rowGap;
    if (props.fontSize != null) s.fontSize = String(props.fontSize).indexOf("px") >= 0 ? props.fontSize : props.fontSize + "px";
    if (props.fontWeight != null) s.fontWeight = props.fontWeight;
    if (props.fontFamily != null) s.fontFamily = props.fontFamily;
    if (props.width != null) s.width = String(props.width).toLowerCase() === "fill" ? "100%" : props.width;
    if (props.height != null) s.height = String(props.height).toLowerCase() === "fill" ? "100%" : props.height;
    if (props.grow != null) s.flexGrow = String(props.grow);
    if (props.alignSelf != null) s.alignSelf = props.alignSelf === "Center" ? "center" : props.alignSelf;
    if (props.alignItems != null) s.alignItems = props.alignItems;
    if (props.orientation) s.flexDirection = String(props.orientation).toLowerCase() === "horizontal" ? "row" : "column";
    if (props.columns) {
      s.display = "grid";
      s.gridTemplateColumns = String(props.columns).split(",").map(function (c) {
        var t = c.trim();
        if (t === "*") return "1fr";
        if (t.toLowerCase() === "auto") return "auto";
        if (/^\d+$/.test(t)) return t + "px";
        return t;
      }).join(" ");
    }
    if (props.rows) {
      s.display = "grid";
      s.gridTemplateRows = String(props.rows).split(",").map(function (c) {
        var t = c.trim();
        if (t === "*") return "1fr";
        if (t.toLowerCase() === "auto") return "auto";
        if (/^\d+$/.test(t)) return t + "px";
        return t;
      }).join(" ");
    }
    if (props.gridColumnSpan != null) s.gridColumn = "span " + props.gridColumnSpan;
    if (props.gridColumn != null) s.gridColumn = String(Number(props.gridColumn) + 1);
  }

  function render(vnode, container) {
    if (vnode == null || vnode === false) return;
    if (typeof vnode === "string" || typeof vnode === "number") {
      container.appendChild(document.createTextNode(String(vnode)));
      return;
    }
    if (Array.isArray(vnode)) {
      vnode.forEach(function (v) { render(v, container); });
      return;
    }
    if (typeof vnode.type === "function") {
      render(vnode.type(Object.assign({}, vnode.props, { children: vnode.children })), container);
      return;
    }
    var tag = "div";
    if (vnode.type === "button" || vnode.props && vnode.props._tag === "button") tag = "button";
    if (vnode.type === "input" || vnode.props && vnode.props._tag === "input") tag = "input";
    if (vnode.type === "span") tag = "span";
    var el = document.createElement(tag);
    var props = vnode.props || {};
    if (props.id) el.id = props.id;
    if (props.className) el.className = props.className;
    if (props.action) el.setAttribute("data-action", props.action);
    if (tag === "button") el.type = "button";
    if (tag === "input") {
      el.type = "text";
      if (props.placeholder) el.placeholder = props.placeholder;
      if (props.text) el.value = props.text;
    }
    applyStyle(el, props);
    var label = props.text != null ? props.text : props.content;
    if (label != null && (!vnode.children || !vnode.children.length) && tag !== "input") el.textContent = String(label);
    (vnode.children || []).forEach(function (c) { render(c, el); });
    container.appendChild(el);
  }

  function Dashboard(p) { return h("div", Object.assign({ className: "ui-dashboard" }, p), p.children); }
  function Taskbar(p) { return h("div", Object.assign({ className: "taskbar" }, p), p.children); }
  function Window(p) { return h("div", Object.assign({ className: "ui-window-frame" }, p), p.children); }
  function Panel(p) { return h("div", Object.assign({ className: "ui-panel" }, p), p.children); }
  function Card(p) { return h("div", Object.assign({ className: "ui-card" }, p), p.children); }
  function Stack(p) { return h("div", Object.assign({ className: "ui-stack" }, p), p.children); }
  function Grid(p) { return h("div", Object.assign({ className: "ui-grid" }, p), p.children); }
  function Button(p) {
    var kids = (p.children && p.children.length) ? p.children : [p.content || p.text || ""];
    return h("button", Object.assign({ className: "ui-button", _tag: "button" }, p), kids);
  }
  function Badge(p) { return h("span", Object.assign({ className: "ui-badge" }, p), p.text || "", p.children); }
  function Input(p) { return h("input", Object.assign({ className: "ui-input", _tag: "input" }, p)); }
  function List(p) { return h("div", Object.assign({ className: "ui-list" }, p), p.children); }
  function Text(p) { return h("div", Object.assign({ className: "ui-text" }, p), p.text || "", p.children); }
  function Icon(p) { return h("span", Object.assign({ className: "ui-icon" }, p), p.icon || "", p.children); }
  function Keypad(p) { return h("div", Object.assign({ className: "ui-keypad" }, p), p.children); }
  function Tile(p) { return h("div", Object.assign({ className: "app-tile" }, p), p.children); }
  function Header(p) { return h("div", Object.assign({ className: "ui-header" }, p), p.text || "", p.children); }
  function Footer(p) { return h("div", Object.assign({ className: "ui-footer" }, p), p.children); }
  function Row(p) {
    return h("div", Object.assign({ className: "ui-row" }, p),
      h("div", { className: "title" }, p.title || ""),
      h("div", { className: "subtitle" }, p.subtitle || ""),
      p.children);
  }
  function Screen(p) { return h("div", Object.assign({ className: "ui-screen" }, p), p.children); }

  g.h = h;
  g.renderVNode = render;
  g.Dashboard = Dashboard;
  g.Taskbar = Taskbar;
  g.Window = Window;
  g.Panel = Panel;
  g.Card = Card;
  g.Stack = Stack;
  g.Grid = Grid;
  g.Button = Button;
  g.Badge = Badge;
  g.Input = Input;
  g.List = List;
  g.Text = Text;
  g.Icon = Icon;
  g.Keypad = Keypad;
  g.Tile = Tile;
  g.Header = Header;
  g.Footer = Footer;
  g.Row = Row;
  g.Screen = Screen;
})(window);
