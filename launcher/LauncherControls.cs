using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Windows.Forms;

internal static class LauncherStyle
{
    public static readonly Color Field = Color.FromArgb(22, 26, 28);
    public static readonly Color Text = Color.FromArgb(237, 238, 235);
    public static readonly Color Muted = Color.FromArgb(159, 166, 170);
    public static readonly Color Yellow = Color.FromArgb(248, 192, 11);
    public static readonly Color Border = Color.FromArgb(139, 146, 149);

    public static Font BodyFont(float pixels)
    {
        return new Font("Bahnschrift SemiCondensed", pixels, FontStyle.Regular, GraphicsUnit.Pixel);
    }

    public static void Prepare(Graphics g)
    {
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
    }

    public static void DrawText(Graphics g, string text, Font font, Color color, RectangleF bounds, StringAlignment alignment)
    {
        using (Brush brush = new SolidBrush(color))
        using (StringFormat format = new StringFormat(StringFormat.GenericTypographic))
        {
            format.Alignment = alignment;
            format.LineAlignment = StringAlignment.Center;
            format.Trimming = StringTrimming.EllipsisCharacter;
            format.FormatFlags |= StringFormatFlags.NoWrap;
            foreach (char character in text)
                if (character >= '\u0600' && character <= '\u08ff') {
                    format.FormatFlags |= StringFormatFlags.DirectionRightToLeft;
                    if (alignment == StringAlignment.Near) format.Alignment = StringAlignment.Far;
                    break;
                }
            float width = g.MeasureString(text, font, 10000, format).Width;
            if (width > bounds.Width && bounds.Width > 0)
            {
                float factor = Math.Max(.62F, bounds.Width / width);
                using (Font fitted = new Font(font.FontFamily, font.Size * factor, font.Style, font.Unit))
                    g.DrawString(text, fitted, brush, bounds, format);
            }
            else g.DrawString(text, font, brush, bounds, format);
        }
    }

    public static void Chevron(Graphics g, Rectangle bounds, bool up, Color color)
    {
        float x = bounds.Left + bounds.Width / 2F;
        float y = bounds.Top + bounds.Height / 2F;
        float dx = Math.Max(3F, bounds.Width * .13F);
        float dy = dx * .6F;
        using (Pen pen = new Pen(color, 2F))
        {
            pen.StartCap = LineCap.Round;
            pen.EndCap = LineCap.Round;
            float sign = up ? -1F : 1F;
            g.DrawLines(pen, new[] { new PointF(x - dx, y - sign * dy), new PointF(x, y + sign * dy), new PointF(x + dx, y - sign * dy) });
        }
    }
}

// Same colors, border, rounded corners and heading font as the native vote buttons.
internal sealed class VoteStyleSettingsButton : Button
{
    private readonly bool primary;
    private bool hovered, pressed;
    internal VoteStyleSettingsButton(bool primary)
    {
        this.primary=primary;
        SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);
        FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;UseVisualStyleBackColor=false;Cursor=Cursors.Hand;
        AccessibleRole=AccessibleRole.PushButton;
    }
    protected override void OnMouseEnter(EventArgs e){hovered=true;Invalidate();base.OnMouseEnter(e);}
    protected override void OnMouseLeave(EventArgs e){hovered=false;pressed=false;Invalidate();base.OnMouseLeave(e);}
    protected override void OnMouseDown(MouseEventArgs e){if(e.Button==MouseButtons.Left){pressed=true;Invalidate();}base.OnMouseDown(e);}
    protected override void OnMouseUp(MouseEventArgs e){pressed=false;Invalidate();base.OnMouseUp(e);}
    protected override void OnEnabledChanged(EventArgs e){base.OnEnabledChanged(e);Invalidate();}
    protected override void OnGotFocus(EventArgs e){base.OnGotFocus(e);Invalidate();}
    protected override void OnLostFocus(EventArgs e){base.OnLostFocus(e);Invalidate();}
    protected override void OnPaint(PaintEventArgs e)
    {
        var g=e.Graphics;g.Clear(Parent==null?Color.FromArgb(27,28,32):Parent.BackColor);LauncherStyle.Prepare(g);
        if(Width<2||Height<2)return;
        var rect=new Rectangle(0,0,Width-1,Height-1);
        Color border=primary?Color.FromArgb(255,205,52):(hovered||Focused)?NativeArt.Yellow:Color.FromArgb(110,110,116);
        Color top=primary?Color.FromArgb(255,210,35):Color.FromArgb(41,42,46),bottom=primary?Color.FromArgb(239,165,0):Color.FromArgb(20,21,24);
        if(pressed){top=ControlPaint.Dark(top);bottom=ControlPaint.Dark(bottom);}
        using(var path=NativeArt.Path(rect,5))using(var brush=new LinearGradientBrush(rect,top,bottom,90)){g.FillPath(brush,path);using(var pen=new Pen(border))g.DrawPath(pen,path);}
        bool fallback=false;foreach(char c in Text)if(c>0x024f){fallback=true;break;}
        Font font=primary?NativeArt.Heavy(19):NativeArt.Font(13,true,true);
        if(fallback){using(var f=new Font("Segoe UI",primary?16:13,FontStyle.Bold,GraphicsUnit.Pixel))LauncherStyle.DrawText(g,Text,f,Enabled?(primary?Color.FromArgb(8,8,8):NativeArt.Text):NativeArt.Muted,new RectangleF(5,1,Width-10,Height-2),StringAlignment.Center);}
        else LauncherStyle.DrawText(g,Text,font,Enabled?(primary?Color.FromArgb(8,8,8):NativeArt.Text):NativeArt.Muted,new RectangleF(5,1,Width-10,Height-2),StringAlignment.Center);
        if(!Enabled)using(var shade=new SolidBrush(Color.FromArgb(100,NativeArt.Bg)))g.FillRectangle(shade,rect);
    }
}

internal sealed class WindowActionButton : Button
{
    private readonly bool _closeGlyph;
    private bool _hovered;

    public WindowActionButton(bool closeGlyph)
    {
        _closeGlyph = closeGlyph;
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
            ControlStyles.OptimizedDoubleBuffer | ControlStyles.SupportsTransparentBackColor, true);
        BackColor = Color.Transparent;
        FlatStyle = FlatStyle.Flat;
        FlatAppearance.BorderSize = 0;
        Cursor = Cursors.Hand;
        TabStop = true;
    }

    protected override void OnMouseEnter(EventArgs e) { _hovered = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hovered = false; Invalidate(); base.OnMouseLeave(e); }

    protected override void OnPaint(PaintEventArgs e)
    {
        LauncherStyle.Prepare(e.Graphics);
        using (Brush fill = new SolidBrush(_hovered ? Color.FromArgb(225, 32, 35, 37) : Color.FromArgb(170, 15, 18, 20)))
            e.Graphics.FillRectangle(fill, ClientRectangle);
        using (Pen pen = new Pen(_hovered ? LauncherStyle.Yellow : LauncherStyle.Text, 2F)) {
            float x = Width / 2F, y = Height / 2F;
            float radius = Math.Max(4F, Height * .14F);
            if (_closeGlyph) {
                e.Graphics.DrawLine(pen, x - radius, y - radius, x + radius, y + radius);
                e.Graphics.DrawLine(pen, x - radius, y + radius, x + radius, y - radius);
            } else e.Graphics.DrawLine(pen, x - radius, y + radius * .5F, x + radius, y + radius * .5F);
        }
        if (Focused) ControlPaint.DrawFocusRectangle(e.Graphics, Rectangle.Inflate(ClientRectangle, -3, -3));
    }
}

internal sealed class StyledModeSelector : Control
{
    public readonly List<string> Items=new List<string>();
    private int selectedIndex=-1;
    public event EventHandler SelectedIndexChanged;
    public int SelectedIndex {
        get {return selectedIndex;}
        set {if(value < -1 || value>=Items.Count)throw new ArgumentOutOfRangeException("value");if(selectedIndex==value)return;selectedIndex=value;Invalidate();var changed=SelectedIndexChanged;if(changed!=null)changed(this,EventArgs.Empty);}
    }
    public StyledModeSelector(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);BackColor=LauncherStyle.Field;ForeColor=LauncherStyle.Muted;TabStop=true;Cursor=Cursors.Hand;AccessibleRole=AccessibleRole.ComboBox;}
    protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);e.Graphics.Clear(BackColor);LauncherStyle.Prepare(e.Graphics);
        Color color=Enabled ? ForeColor : Color.FromArgb(90,96,99);using(var p=new Pen(Enabled ? LauncherStyle.Border : Color.FromArgb(70,78,82)))e.Graphics.DrawRectangle(p,0,0,Width-1,Height-1);
        if(selectedIndex>=0 && selectedIndex<Items.Count)LauncherStyle.DrawText(e.Graphics,Items[selectedIndex],Font,color,new RectangleF(7,1,Math.Max(1,Width-34),Height-2),StringAlignment.Near);
        int x=Width-Math.Max(14,Height/2),y=Height/2-2;using(var p=new Pen(color,1.6f)){e.Graphics.DrawLine(p,x-4,y,x,y+4);e.Graphics.DrawLine(p,x,y+4,x+4,y);}
    }
    private void OpenMenu(){if(!Enabled || Items.Count==0)return;var menu=new ContextMenuStrip{ShowImageMargin=false,BackColor=LauncherStyle.Field,ForeColor=LauncherStyle.Muted,Font=Font};
        for(int i=0;i<Items.Count;i++){int chosen=i;var item=new ToolStripMenuItem(Items[i]){Checked=i==selectedIndex};item.Click+=delegate{SelectedIndex=chosen;Focus();};menu.Items.Add(item);}
        menu.Closed+=delegate{menu.Dispose();};menu.Show(this,new Point(0,Height));
    }
    protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);if(e.Button==MouseButtons.Left){Focus();OpenMenu();}}
    protected override bool IsInputKey(Keys keyData){Keys key=keyData&Keys.KeyCode;return key==Keys.Up || key==Keys.Down || base.IsInputKey(keyData);}
    protected override void OnKeyDown(KeyEventArgs e){base.OnKeyDown(e);if(e.KeyCode==Keys.Down){SelectedIndex=Math.Min(Items.Count-1,selectedIndex+1);e.Handled=true;}else if(e.KeyCode==Keys.Up){SelectedIndex=Math.Max(0,selectedIndex-1);e.Handled=true;}else if(e.KeyCode==Keys.Enter || e.KeyCode==Keys.Space){OpenMenu();e.Handled=true;}}
}

internal class StyledTextField : UserControl
{
    protected readonly TextBox Editor;
    protected bool Hovered;
    public HorizontalAlignment TextAlign { get { return Editor.TextAlign; } set { Editor.TextAlign = value; } }
    public int MaxLength { get { return Editor.MaxLength; } set { Editor.MaxLength = value; } }
    public override string Text
    {
        get { return Editor == null ? base.Text : Editor.Text; }
        set { if (Editor == null) base.Text = value; else Editor.Text = value; }
    }

    public StyledTextField()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        BackColor = LauncherStyle.Field;
        ForeColor = LauncherStyle.Text;
        AutoScaleMode = AutoScaleMode.None;
        TabStop = false;
        Editor = new TextBox();
        Editor.Name = "Input";
        Editor.AutoSize = false;
        Editor.BorderStyle = BorderStyle.None;
        Editor.BackColor = BackColor;
        Editor.ForeColor = ForeColor;
        Editor.Font = Font;
        Editor.TextChanged += delegate { OnTextChanged(EventArgs.Empty); };
        Editor.Enter += delegate { Invalidate(); };
        Editor.Leave += delegate { Invalidate(); };
        Controls.Add(Editor);
    }

    protected virtual int ReservedRight { get { return 0; } }

    protected virtual void LayoutEditor()
    {
        if (Editor == null) return;
        int pad = Math.Max(9, (int)Math.Round(Height * .23));
        int h = Math.Min(Math.Max(1, Height - 8), Font.Height + 4);
        Editor.SetBounds(pad, (Height - h) / 2, Math.Max(1, Width - pad * 2 - ReservedRight), h);
    }

    public new bool Focus() { return Editor.Focus(); }
    public void SelectAll() { Editor.SelectAll(); }
    protected override void OnEnter(EventArgs e) { base.OnEnter(e); if (Editor != null) Editor.Focus(); }
    protected override void OnResize(EventArgs e) { base.OnResize(e); LayoutEditor(); Invalidate(); }
    protected override void OnFontChanged(EventArgs e) { base.OnFontChanged(e); if (Editor != null) { Editor.Font = Font; LayoutEditor(); } }
    protected override void OnBackColorChanged(EventArgs e) { base.OnBackColorChanged(e); if (Editor != null) Editor.BackColor = BackColor; }
    protected override void OnForeColorChanged(EventArgs e) { base.OnForeColorChanged(e); if (Editor != null) Editor.ForeColor = ForeColor; }
    protected override void OnEnabledChanged(EventArgs e) { base.OnEnabledChanged(e); Invalidate(); }
    protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); Hovered = true; Invalidate(); }
    protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); Hovered = false; Invalidate(); }
    protected override void OnMouseDown(MouseEventArgs e) { base.OnMouseDown(e); if (Enabled) Editor.Focus(); }
    protected override void OnPaintBackground(PaintEventArgs e) { e.Graphics.Clear(BackColor); }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        Color border = !Enabled ? Color.FromArgb(70, 78, 82) : Editor.Focused ? LauncherStyle.Yellow : Hovered ? Color.FromArgb(193, 199, 199) : LauncherStyle.Border;
        using (Pen pen = new Pen(border, Editor.Focused ? 2F : 1F))
            e.Graphics.DrawRectangle(pen, 0, 0, Math.Max(0, Width - 1), Math.Max(0, Height - 1));
    }
}

internal sealed class BotCountSelector : StyledTextField
{
    private int _minimum;
    private int _maximum = 95;
    private int _value = 95;
    private int _hoverArrow;
    private int ArrowWidth { get { return Math.Max(25, (int)Math.Round(Height * .58)); } }
    protected override int ReservedRight { get { return ArrowWidth; } }
    public int Minimum { get { return _minimum; } set { _minimum = value; if (_maximum < value) _maximum = value; Value = _value; } }
    public int Maximum { get { return _maximum; } set { _maximum = Math.Max(_minimum, value); Value = _value; } }
    public int Value
    {
        get { Normalize(); return _value; }
        set { _value = Math.Max(_minimum, Math.Min(_maximum, value)); Editor.Text = _value.ToString(); }
    }

    public BotCountSelector()
    {
        Editor.TextAlign = HorizontalAlignment.Center;
        Editor.MaxLength = 3;
        Editor.Text = "95";
        Editor.KeyPress += delegate(object sender, KeyPressEventArgs e) { if (!Char.IsControl(e.KeyChar) && !Char.IsDigit(e.KeyChar)) e.Handled = true; };
        Editor.KeyDown += delegate(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Up || e.KeyCode == Keys.Down) { Step(e.KeyCode == Keys.Up ? 1 : -1); e.Handled = true; e.SuppressKeyPress = true; }
        };
        Editor.Leave += delegate { Normalize(); };
    }

    private void Normalize()
    {
        int parsed;
        if (!Int32.TryParse(Editor.Text, out parsed)) parsed = _value;
        int next = Math.Max(_minimum, Math.Min(_maximum, parsed));
        _value = next;
        if (Editor.Text != next.ToString()) Editor.Text = next.ToString();
    }

    internal void Step(int delta) { Normalize(); Value = _value + delta; Editor.Focus(); }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        _hoverArrow = e.X >= Width - ArrowWidth ? (e.Y < Height / 2 ? 1 : -1) : 0;
        Cursor = _hoverArrow == 0 ? Cursors.IBeam : Cursors.Hand;
        Invalidate();
    }
    protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); _hoverArrow = 0; Invalidate(); }
    protected override void OnMouseDown(MouseEventArgs e)
    {
        if (Enabled && e.Button == MouseButtons.Left && e.X >= Width - ArrowWidth) Step(e.Y < Height / 2 ? 1 : -1);
        else base.OnMouseDown(e);
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        Graphics g = e.Graphics;
        LauncherStyle.Prepare(g);
        int x = Width - ArrowWidth;
        if (Enabled && _hoverArrow != 0)
            using (Brush hover = new SolidBrush(Color.FromArgb(65, 57, 28)))
                g.FillRectangle(hover, x + 1, _hoverArrow > 0 ? 1 : Height / 2, ArrowWidth - 2, Height / 2 - 1);
        using (Pen pen = new Pen(Color.FromArgb(83, 91, 95)))
        {
            g.DrawLine(pen, x, 1, x, Height - 2);
            g.DrawLine(pen, x + 1, Height / 2, Width - 2, Height / 2);
        }
        Color normal = Enabled ? LauncherStyle.Muted : Color.FromArgb(74, 81, 85);
        LauncherStyle.Chevron(g, new Rectangle(x, 1, ArrowWidth, Height / 2), true, _hoverArrow == 1 ? LauncherStyle.Yellow : normal);
        LauncherStyle.Chevron(g, new Rectangle(x, Height / 2, ArrowWidth, Height / 2), false, _hoverArrow == -1 ? LauncherStyle.Yellow : normal);
    }
}

internal sealed class MapSelector : Control
{
    private readonly List<MapOption> _items = new List<MapOption>();
    private int _selectedIndex = -1;
    private bool _hover;
    private ToolStripDropDown _popup;
    public event EventHandler SelectedIndexChanged;
    public List<MapOption> Items { get { return _items; } }
    public MapOption SelectedItem { get { return _selectedIndex < 0 || _selectedIndex >= _items.Count ? null : _items[_selectedIndex]; } }
    public int SelectedIndex
    {
        get { return _selectedIndex; }
        set
        {
            if (value < -1 || value >= _items.Count) throw new ArgumentOutOfRangeException("value");
            if (_selectedIndex == value) return;
            _selectedIndex = value; Invalidate();
            EventHandler changed = SelectedIndexChanged; if (changed != null) changed(this, EventArgs.Empty);
        }
    }
    public MapSelector()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.Selectable, true);
        BackColor = LauncherStyle.Field; ForeColor = LauncherStyle.Text; TabStop = true; Cursor = Cursors.Hand;
    }
    protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); _hover = true; Invalidate(); }
    protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); _hover = false; Invalidate(); }
    protected override void OnGotFocus(EventArgs e) { base.OnGotFocus(e); Invalidate(); }
    protected override void OnLostFocus(EventArgs e) { base.OnLostFocus(e); Invalidate(); }
    protected override void OnEnabledChanged(EventArgs e) { base.OnEnabledChanged(e); if (!Enabled && _popup != null) _popup.Close(); Invalidate(); }
    protected override void OnClick(EventArgs e) { base.OnClick(e); Focus(); ShowPopup(); }
    protected override bool IsInputKey(Keys keyData) { return keyData == Keys.Up || keyData == Keys.Down || base.IsInputKey(keyData); }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        if (e.KeyCode == Keys.Space || e.KeyCode == Keys.Enter || (e.Alt && e.KeyCode == Keys.Down)) { ShowPopup(); e.Handled = true; }
        else if (e.KeyCode == Keys.Down) { SelectedIndex = Math.Min(_items.Count - 1, _selectedIndex + 1); e.Handled = true; }
        else if (e.KeyCode == Keys.Up) { SelectedIndex = Math.Max(0, _selectedIndex - 1); e.Handled = true; }
    }
    internal void ShowPopup()
    {
        if (!Enabled || _items.Count == 0) return;
        if (_popup != null && _popup.Visible) { _popup.Close(); return; }
        if (_popup != null) _popup.Dispose();
        int rowHeight = Math.Max(40, (int)Math.Round(Height * 1.1));
        MapMenu menu = new MapMenu(_items, _selectedIndex, Width, rowHeight, Font);
        ToolStripControlHost host = new ToolStripControlHost(menu);
        host.AutoSize = false; host.Size = menu.Size; host.Margin = Padding.Empty; host.Padding = Padding.Empty;
        _popup = new ToolStripDropDown();
        _popup.AutoSize = false; _popup.Padding = Padding.Empty; _popup.Margin = Padding.Empty;
        _popup.BackColor = BackColor; _popup.DropShadowEnabled = true; _popup.Items.Add(host); _popup.Size = menu.Size;
        _popup.Closed += delegate { Invalidate(); };
        menu.Chosen += delegate(int index) { SelectedIndex = index; _popup.Close(); Focus(); };
        menu.Cancelled += delegate { _popup.Close(); Focus(); };
        _popup.Show(this, new Point(0, Height + 3));
        menu.Focus(); Invalidate();
    }
    protected override void Dispose(bool disposing) { if (disposing && _popup != null) _popup.Dispose(); base.Dispose(disposing); }
    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics; g.Clear(BackColor); LauncherStyle.Prepare(g);
        float scale = Height / 60F;
        MapOption option = SelectedItem;
        if (option != null)
        {
            Rectangle thumbnail = new Rectangle(3, 3, (int)Math.Round(158 * scale), Height - 6);
            g.DrawImage(option.Preview, thumbnail);
            if (!Enabled) using (Brush shade = new SolidBrush(Color.FromArgb(140, BackColor))) g.FillRectangle(shade, thumbnail);
            int tx = thumbnail.Right + (int)(22 * scale);
            LauncherStyle.DrawText(g, option.Label, Font, Enabled ? ForeColor : LauncherStyle.Muted,
                new RectangleF(tx, 0, Math.Max(1, Width - tx - Height), Height), StringAlignment.Near);
        }
        bool active = Enabled && (_hover || Focused || (_popup != null && _popup.Visible));
        using (Pen pen = new Pen(active ? LauncherStyle.Yellow : Enabled ? LauncherStyle.Border : Color.FromArgb(70, 78, 82), active ? 2F : 1F))
            g.DrawRectangle(pen, 0, 0, Width - 1, Height - 1);
        LauncherStyle.Chevron(g, new Rectangle(Width - Height, 0, Height, Height), false, active ? LauncherStyle.Yellow : LauncherStyle.Muted);
    }
}

internal sealed class MapMenu : Control
{
    private readonly List<MapOption> _items;
    private readonly int _rowHeight;
    private int _hot;
    public event Action<int> Chosen;
    public event Action Cancelled;
    public MapMenu(List<MapOption> items, int selected, int width, int rowHeight, Font font)
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.Selectable, true);
        _items = items; _rowHeight = rowHeight; _hot = Math.Max(0, selected); Font = font;
        Size = new Size(width, rowHeight * items.Count); BackColor = LauncherStyle.Field; Cursor = Cursors.Hand; TabStop = true;
    }
    protected override void OnMouseMove(MouseEventArgs e) { base.OnMouseMove(e); int index = Math.Max(0, Math.Min(_items.Count - 1, e.Y / _rowHeight)); if (_hot != index) { _hot = index; Invalidate(); } }
    protected override void OnMouseDown(MouseEventArgs e) { base.OnMouseDown(e); if (e.Button == MouseButtons.Left && Chosen != null) Chosen(Math.Max(0, Math.Min(_items.Count - 1, e.Y / _rowHeight))); }
    protected override bool IsInputKey(Keys keyData) { return keyData == Keys.Up || keyData == Keys.Down || base.IsInputKey(keyData); }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        if (e.KeyCode == Keys.Up) _hot = Math.Max(0, _hot - 1);
        else if (e.KeyCode == Keys.Down) _hot = Math.Min(_items.Count - 1, _hot + 1);
        else if (e.KeyCode == Keys.Enter && Chosen != null) Chosen(_hot);
        else if (e.KeyCode == Keys.Escape && Cancelled != null) Cancelled();
        else return;
        e.Handled = true; Invalidate();
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics; LauncherStyle.Prepare(g); g.Clear(BackColor);
        float scale = _rowHeight / 66F;
        for (int i = 0; i < _items.Count; i++)
        {
            Rectangle row = new Rectangle(1, i * _rowHeight, Width - 2, _rowHeight);
            using (Brush brush = new SolidBrush(i == _hot ? Color.FromArgb(57, 51, 26) : Color.FromArgb(15, 20, 23))) g.FillRectangle(brush, row);
            Rectangle thumbnail = new Rectangle((int)(10 * scale), row.Y + (int)(6 * scale), (int)(182 * scale), _rowHeight - (int)(12 * scale));
            g.DrawImage(_items[i].Preview, thumbnail);
            LauncherStyle.DrawText(g, _items[i].Label, Font, LauncherStyle.Text,
                new RectangleF(thumbnail.Right + 23 * scale, row.Top, Math.Max(1, Width - thumbnail.Right - 30 * scale), row.Height), StringAlignment.Near);
            using (Pen line = new Pen(i == _hot ? LauncherStyle.Yellow : Color.FromArgb(53, 62, 66)))
            {
                if (i == _hot) g.DrawRectangle(line, row.X, row.Y, row.Width - 1, row.Height - 1);
                else g.DrawLine(line, row.Left, row.Bottom - 1, row.Right, row.Bottom - 1);
            }
        }
        using (Pen border = new Pen(LauncherStyle.Border)) g.DrawRectangle(border, 0, 0, Width - 1, Height - 1);
    }
}

internal sealed class ArtworkCheckBox : Control
{
    private bool _checked;
    public event EventHandler CheckedChanged;
    public bool Checked
    {
        get { return _checked; }
        set { if (_checked == value) return; _checked = value; Invalidate(); EventHandler handler = CheckedChanged; if (handler != null) handler(this, EventArgs.Empty); }
    }
    public ArtworkCheckBox()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.Selectable, true);
        TabStop = true; Cursor = Cursors.Hand;
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics; LauncherStyle.Prepare(g);
        if (BackgroundImage != null) g.DrawImage(BackgroundImage, ClientRectangle); else g.Clear(BackColor);
        float scale = Height / 54F;
        int size = (int)Math.Round(34 * scale);
        Rectangle box = new Rectangle((int)Math.Round(10 * scale), (int)Math.Round(9 * scale), size, size);
        using (Brush fill = new SolidBrush(_checked && Enabled ? LauncherStyle.Yellow : LauncherStyle.Field)) g.FillRectangle(fill, box);
        using (Pen border = new Pen(Enabled && (_checked || Focused) ? LauncherStyle.Yellow : LauncherStyle.Border, 1.4F)) g.DrawRectangle(border, box);
        if (_checked)
            using (Pen check = new Pen(Color.FromArgb(19, 24, 25), Math.Max(2F, 3 * scale)))
            {
                check.StartCap = LineCap.Round; check.EndCap = LineCap.Round;
                g.DrawLines(check, new[] { new PointF(box.Left + size * .2F, box.Top + size * .5F), new PointF(box.Left + size * .43F, box.Top + size * .74F), new PointF(box.Left + size * .82F, box.Top + size * .23F) });
            }
        if (Focused) using (Pen pen = new Pen(Color.FromArgb(100, LauncherStyle.Yellow))) g.DrawRectangle(pen, box.Left - 3, box.Top - 3, box.Width + 6, box.Height + 6);
        int textLeft = box.Right + (int)Math.Round(15 * scale);
        LauncherStyle.DrawText(g, Text, Font, Enabled ? LauncherStyle.Text : LauncherStyle.Muted,
            new RectangleF(textLeft, 0, Math.Max(1, Width - textLeft), Height), StringAlignment.Near);
    }
    protected override void OnEnabledChanged(EventArgs e) { base.OnEnabledChanged(e); Invalidate(); }
    protected override void OnClick(EventArgs e) { base.OnClick(e); if (Enabled) { Focus(); Checked = !Checked; } }
    protected override void OnKeyDown(KeyEventArgs e) { base.OnKeyDown(e); if (e.KeyCode == Keys.Space) { Checked = !Checked; e.Handled = true; } }
}

internal sealed class ArtworkButton : Button
{
    private bool _hover;
    public ArtworkButton() { SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true); Cursor = Cursors.Hand; }
    protected override void OnMouseEnter(EventArgs e) { base.OnMouseEnter(e); _hover = true; Invalidate(); }
    protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); _hover = false; Invalidate(); }
    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics; LauncherStyle.Prepare(g);
        if (BackgroundImage != null) g.DrawImage(BackgroundImage, ClientRectangle); else g.Clear(LauncherStyle.Yellow);
        if (_hover || !Enabled) using (Brush tint = new SolidBrush(Enabled ? Color.FromArgb(20, Color.White) : Color.FromArgb(90, Color.Black))) g.FillRectangle(tint, ClientRectangle);
        Color text = Enabled ? Color.FromArgb(16, 21, 22) : Color.FromArgb(76, 71, 41);
        LauncherStyle.DrawText(g, Text, Font, text, new RectangleF(14, 0, Width - 28, Height - 3), StringAlignment.Center);
        if (Focused) using (Pen border = new Pen(Color.FromArgb(100, 30, 26, 9))) { border.DashStyle = DashStyle.Dot; g.DrawRectangle(border, 5, 5, Width - 11, Height - 11); }
    }
}

internal sealed class ArtworkLabel : Control
{
    public ArtworkLabel() { SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true); }
    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics; LauncherStyle.Prepare(g);
        if (BackgroundImage != null) g.DrawImage(BackgroundImage, ClientRectangle); else g.Clear(BackColor);
        LauncherStyle.DrawText(g, Text, Font, ForeColor, new RectangleF(0, 0, Width, Height), StringAlignment.Near);
    }
}

internal sealed class LauncherLogView : Control
{
    private readonly List<string> _lines = new List<string>();
    private int _scrollOffset;
    public Func<string, string> LineTransform { get; set; }
    public LauncherLogView() { SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.Selectable, true); TabStop = true; }
    public void AppendLine(string line) { _lines.Add(line); if (_scrollOffset > 0) _scrollOffset++; if (_lines.Count > 1000) _lines.RemoveAt(0); _scrollOffset = Math.Min(_scrollOffset, _lines.Count - 1); Invalidate(); }
    public string DisplayLine(string line) { return LineTransform == null ? line : LineTransform(line); }
    protected override void OnMouseDown(MouseEventArgs e) { base.OnMouseDown(e); Focus(); }
    protected override void OnMouseWheel(MouseEventArgs e)
    {
        base.OnMouseWheel(e);
        _scrollOffset = Math.Max(0, Math.Min(Math.Max(0, _lines.Count - 1), _scrollOffset + (e.Delta > 0 ? 3 : -3)));
        Invalidate();
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics; LauncherStyle.Prepare(g);
        if (BackgroundImage != null) g.DrawImage(BackgroundImage, ClientRectangle); else g.Clear(BackColor);
        int rowHeight = Font.Height + 4;
        int count = Math.Max(1, Height / rowHeight);
        int end = Math.Max(0, _lines.Count - _scrollOffset);
        int start = Math.Max(0, end - count);
        using (Brush brush = new SolidBrush(ForeColor))
        using (StringFormat format = new StringFormat())
        {
            format.FormatFlags = StringFormatFlags.NoWrap; format.Trimming = StringTrimming.EllipsisCharacter;
            for (int i = start; i < end; i++)
            {
                string line = DisplayLine(_lines[i]);
                string stamp = line.Length >= 10 ? line.Substring(0, 10) : "";
                string message = line.Substring(stamp.Length);
                float stampWidth = g.MeasureString(stamp, Font, 10000, StringFormat.GenericTypographic).Width;
                float y = (i - start) * rowHeight;
                g.DrawString(stamp, Font, brush, new RectangleF(0, y, stampWidth + 2, rowHeight), format);
                bool rtl = false;
                foreach (char character in message) if (character >= '\u0600' && character <= '\u08ff') { rtl = true; break; }
                format.FormatFlags = StringFormatFlags.NoWrap | (rtl ? StringFormatFlags.DirectionRightToLeft : 0);
                format.Alignment = rtl ? StringAlignment.Far : StringAlignment.Near;
                g.DrawString(message, Font, brush, new RectangleF(stampWidth, y, Math.Max(0, Width - stampWidth), rowHeight), format);
                format.FormatFlags = StringFormatFlags.NoWrap;
                format.Alignment = StringAlignment.Near;
            }
        }
    }
}
