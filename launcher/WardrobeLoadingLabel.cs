using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

// Native artwork effects; repaint only the small animated area.
internal sealed class WardrobeLoadingLabel : Label {
    private readonly System.Windows.Forms.Timer animationTimer = new System.Windows.Forms.Timer();
    private readonly Stopwatch clock = Stopwatch.StartNew();
    private enum AnimationMode { None, Loading, Closing }
    private AnimationMode mode;
    private const float LoadingLeft=490f,LoadingRight=1180f,LoadingTop=861f,LoadingBottom=887f,LoadingLineY=874f;
    private const float ClosingLeft=535f,ClosingRight=1135f,ClosingTop=837f,ClosingBottom=872f,ClosingLineY=852f;

    internal WardrobeLoadingLabel(Image picture) {
        SetStyle(ControlStyles.OptimizedDoubleBuffer|ControlStyles.AllPaintingInWmPaint|ControlStyles.ResizeRedraw,true);
        BackgroundImage=picture;BackgroundImageLayout=ImageLayout.Zoom;
        mode=IsExpectedArtwork(picture)?AnimationMode.Loading:AnimationMode.None;
        animationTimer.Interval=40;animationTimer.Tick+=delegate{if(mode!=AnimationMode.None&&Visible)Invalidate(AnimationBounds());};
    }
    private static bool IsExpectedArtwork(Image image){return image!=null&&image.Width==1672&&image.Height==941;}
    private RectangleF ImageBounds(){
        var image=BackgroundImage;if(image==null||ClientSize.Width<=0||ClientSize.Height<=0)return RectangleF.Empty;
        float scale=Math.Min((float)ClientSize.Width/image.Width,(float)ClientSize.Height/image.Height);
        float w=image.Width*scale,h=image.Height*scale;
        return new RectangleF((ClientSize.Width-w)/2f,(ClientSize.Height-h)/2f,w,h);
    }
    private Rectangle AnimationBounds(){
        var image=BackgroundImage;var b=ImageBounds();if(image==null||b.IsEmpty)return Rectangle.Empty;
        float s=b.Width/image.Width;
        if(mode==AnimationMode.Closing)return Rectangle.Ceiling(new RectangleF(b.X+ClosingLeft*s,b.Y+ClosingTop*s,(ClosingRight-ClosingLeft)*s,(ClosingBottom-ClosingTop)*s));
        return Rectangle.Ceiling(new RectangleF(b.X+LoadingLeft*s,b.Y+LoadingTop*s,(LoadingRight-LoadingLeft)*s,(LoadingBottom-LoadingTop)*s));
    }
    protected override void OnVisibleChanged(EventArgs e){base.OnVisibleChanged(e);if(mode!=AnimationMode.None&&Visible)animationTimer.Start();else animationTimer.Stop();}
    protected override void OnPaintBackground(PaintEventArgs e){}
    protected override void OnPaint(PaintEventArgs e){
        e.Graphics.FillRectangle(Brushes.Black,e.ClipRectangle);
        var image=BackgroundImage;var bounds=ImageBounds();
        if(image!=null&&!bounds.IsEmpty)e.Graphics.DrawImage(image,bounds);
        else if(!String.IsNullOrEmpty(Text))TextRenderer.DrawText(e.Graphics,Text,Font,ClientRectangle,ForeColor,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);
        if(mode==AnimationMode.None||!IsExpectedArtwork(image)||bounds.IsEmpty)return;
        float scale=bounds.Width/image.Width;
        if(mode==AnimationMode.Loading)DrawLoadingGlint(e.Graphics,bounds,scale);
        else if(mode==AnimationMode.Closing)DrawClosingGlint(e.Graphics,bounds,scale);
    }
    private void DrawLoadingGlint(Graphics g,RectangleF b,float scale){
        var state=g.Save();try{
            g.SetClip(new RectangleF(b.X+LoadingLeft*scale,b.Y+LoadingTop*scale,(LoadingRight-LoadingLeft)*scale,(LoadingBottom-LoadingTop)*scale));
            float phase=(float)((clock.Elapsed.TotalSeconds%2.6)/2.6);
            float x=b.X+(LoadingLeft+(LoadingRight-LoadingLeft)*phase)*scale,y=b.Y+LoadingLineY*scale;
            GlowStripe(g,x,y,175f*scale,14f*scale,35);
            GlowStripe(g,x,y,110f*scale,4f*scale,175);
            GlowStripe(g,x,y,45f*scale,2f*scale,220);
        }finally{g.Restore(state);}
    }
    private static void GlowStripe(Graphics g,float x,float y,float width,float height,int alpha){
        var rect=new RectangleF(x-width/2f,y-height/2f,width,height);
        using(var brush=new LinearGradientBrush(rect,Color.Transparent,Color.Transparent,LinearGradientMode.Horizontal)){
            var blend=new ColorBlend(5);blend.Positions=new float[]{0f,.33f,.5f,.67f,1f};
            blend.Colors=new Color[]{Color.FromArgb(0,255,174,0),Color.FromArgb(alpha/2,255,190,30),Color.FromArgb(alpha,255,245,175),Color.FromArgb(alpha/2,255,190,30),Color.FromArgb(0,255,174,0)};brush.InterpolationColors=blend;g.FillRectangle(brush,rect);
        }
    }
    private void DrawClosingGlint(Graphics g,RectangleF b,float scale){
        var state=g.Save();try{
            g.SetClip(new RectangleF(b.X+ClosingLeft*scale,b.Y+ClosingTop*scale,(ClosingRight-ClosingLeft)*scale,(ClosingBottom-ClosingTop)*scale));
            float phase=(float)((clock.Elapsed.TotalSeconds%2.6)/2.6);
            float x=b.X+(ClosingLeft+(ClosingRight-ClosingLeft)*phase)*scale,y=b.Y+ClosingLineY*scale;
            GlowStripe(g,x,y,175f*scale,20f*scale,35);
            GlowStripe(g,x,y,110f*scale,5f*scale,175);
            GlowStripe(g,x,y,45f*scale,2f*scale,220);
        }finally{g.Restore(state);}
    }
    internal void BeginClosingAnimation(Image picture){BackgroundImage=picture;mode=AnimationMode.None;clock.Restart();if(mode!=AnimationMode.None&&Visible)animationTimer.Start();else animationTimer.Stop();Invalidate();}
    internal void StopAnimation(){mode=AnimationMode.None;animationTimer.Stop();}
    protected override void Dispose(bool disposing){if(disposing){StopAnimation();animationTimer.Dispose();}base.Dispose(disposing);}
}
