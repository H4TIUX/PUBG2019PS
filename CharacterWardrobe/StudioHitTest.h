#pragma once
// Match the wardrobe's left rail, right inventory panel and bottom action bar.
static bool StudioDragHit(int x,int y,int width,int height){
    if(width<=0||height<=0)return false;
    const bool compact=width<=1200;
    const int left=compact?194:270;
    const int rightMargin=compact?10:20;
    const float inventoryWidth=compact?.43f:.40f;
    const int right=int(width*(1.f-inventoryWidth))-rightMargin;
    const int top=height<=800?58:70;
    const int footer=height<=800?60:72;
    return x>=left&&x<right&&y>=top&&y<height-footer;
}
