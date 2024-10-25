#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#define PALETTE_SIZE 4
#define SEGMENT_LEN 100
#define RUMBLE_LEN 3
#define WINDOW_WIDTH 480
#define WINDOW_HEIGHT 272
#define DRAW_DISTANCE 150
#define ROAD_WIDTH 1000
#define CAM_HEIGHT 2000
#define FOV 100
#define SPEED 100
#define LANES 3

#define LEN_NONE 0
#define LEN_SHORT 25
#define LEN_MEDIUM 50
#define LEN_LONG 100

#define CURVE_NONE 0
#define CURVE_EASY 2
#define CURVE_MEDIUM 4
#define CURVE_HARD 6

#define HILL_NONE 0
#define HILL_LOW 20
#define HILL_MEDIUM 40
#define HILL_HIGH 60

#define easeIn(a,b,p) (a+(b-a)*powf(p,2))
#define easeOut(a,b,p) (a+(b-a)*(1-powf(1-p,2)))
#define easeInOut(a,b,p) (float)((float)a+(b-a)*((float)(-cosf(p*M_PI)/2)+0.5))
#define percentRemaining(n, total) ((float)(n%total)/total)
#define interpolate(a,b,p) (a+(b-a)*p)

int colours_DARK[PALETTE_SIZE] = {0xff2fa31d,0xff667064,0xffffffff,0xffffffff};
int colours_LIGHT[PALETTE_SIZE] = {0xff3edb25,0xff667064,0xff0000ff,0xff667064};

typedef struct vec3{

    float x;
    float y;
    float z;

}vec3;

vec3 zero = {0,0,0};


typedef struct point{
    
    vec3 world;
    vec3 camera;
    vec3 screen;

}point;

typedef struct segment{

    point p1;
    point p2;
    int c;
    int index;
    int* colours;

}segment;

segment road_segments[1000];
int road_len = 0;
int trackLen = 0;
float position = 0;
float cam_depth;

float lastY(){
    return (road_len == 0)?0:road_segments[road_len-1].p2.world.y;
}

void addSegment(int c,float y){
    
    segment s;
    printf("y: %f",y);
    s.index = road_len;
    s.p1.world = zero;
    s.p1.world.y = lastY();
    s.p1.world.z = road_len*SEGMENT_LEN;
    s.p2.world = zero;
    s.p2.world.y = y;
    s.p2.world.z = (road_len+1)*SEGMENT_LEN;
    s.c = c;
    s.colours = ((road_len/RUMBLE_LEN)%2 ? colours_DARK : colours_LIGHT);
    road_segments[road_len] = s;
    road_len++;
}

void addRoad(float enter,float hold,float leave, int curve,float y){
    
    float startY = lastY();
    float endY = startY + (y*SEGMENT_LEN);
    float total =enter+hold+leave;
    int i;
    for(i = 0;i<enter;i++){
        addSegment(easeIn(0,curve,i/enter),easeInOut(startY,endY,(float)i/total));}
    for(i = 0; i<hold;i++){
        addSegment(curve,easeInOut(startY,endY,(enter+(float)i)/total));}
    for(i = 0; i<leave;i++){
        addSegment(easeInOut(curve,0,i/leave),easeInOut(startY,endY,(enter+hold+(float)i)/total));}
    
}

void addSCurves(){
    addRoad(LEN_MEDIUM,LEN_MEDIUM,LEN_MEDIUM,CURVE_EASY,HILL_LOW);
    addRoad(LEN_MEDIUM,LEN_MEDIUM,LEN_MEDIUM,CURVE_EASY,HILL_MEDIUM);
     trackLen = road_len * SEGMENT_LEN;
}

void reset_road(){
    int i;
    
    for (i = 0; i<500; i++){
        segment s;
        point p1;
        p1.world = zero;
        p1.world.z = i*SEGMENT_LEN;

        s.p1 = p1;
        point p2;
        p2.world = zero;
        p2.world.z = (i+1)*SEGMENT_LEN;

        s.p2 = p2;
        s.colours = ((i/RUMBLE_LEN)%2 ? colours_DARK : colours_LIGHT);
        s.index = i;
        road_segments[i] = s;
        road_len++;
    }

    trackLen = road_len * SEGMENT_LEN;
}

segment find_segment(float z){
    return road_segments[(int)(z/SEGMENT_LEN) % road_len];
    
}

void project_point(point* p,int camx, int camy, int camz){

    p->camera.x = (p->world.x ) - camx;
    p->camera.y = (p->world.y ) - camy;
    p->camera.z = (p->world.z ) - camz;

    float screen_scale = cam_depth/p->camera.z;
    p->screen.x = round((WINDOW_WIDTH/2) + (screen_scale *  p->camera.x * WINDOW_WIDTH/2));
    p->screen.y = round((WINDOW_HEIGHT/2) - (screen_scale * p->camera.y  * WINDOW_HEIGHT/2));
   // p->screen.y = p->screen.y < 0 ? 0 : p->screen.y;
    p->screen.z = round((screen_scale * ROAD_WIDTH * WINDOW_WIDTH/2));

}

void render_rumble(segment s,SDL_Renderer* rend){

    float r1 = s.p1.screen.z/6;
    float r2 = s.p2.screen.z/6;

    Sint16 rxs[4] = {s.p1.screen.x -s.p1.screen.z-r1 ,s.p1.screen.x-s.p1.screen.z ,
                     s.p2.screen.x-s.p2.screen.z ,s.p2.screen.x - s.p2.screen.z -r2};
    Sint16 rys[4] = {s.p1.screen.y,s.p1.screen.y,
                     s.p2.screen.y,s.p2.screen.y};
    filledPolygonColor(rend,rxs,rys,4,s.colours[2]);

    Sint16 r2xs[4] = {s.p1.screen.x +s.p1.screen.z+r1 ,s.p1.screen.x+s.p1.screen.z ,
                      s.p2.screen.x+s.p2.screen.z ,s.p2.screen.x + s.p2.screen.z + r2};
    Sint16 r2ys[4] = {s.p1.screen.y,s.p1.screen.y,
                      s.p2.screen.y,s.p2.screen.y};
    filledPolygonColor(rend,r2xs,r2ys,4,s.colours[2]);
}

void render_lanes(segment s,SDL_Renderer* rend){

    float l1 = s.p1.screen.z/32;
    float l2 = s.p2.screen.z/32;

    float lanew1 = s.p1.screen.z*2/LANES;
    float lanew2 = s.p2.screen.z*2/LANES;
    float lanex1 = s.p1.screen.x-s.p1.screen.z + lanew1;
    float lanex2 = s.p2.screen.x-s.p2.screen.z + lanew2;

    for(int i = 1; i<LANES;lanex1+= lanew1,lanex2+=lanew2,i++){
        Sint16 lxs[4] = {lanex1 - l1/2,lanex1+l1/2,lanex2+l2/2,lanex2-l2/2};
        Sint16 lys[4] = {s.p1.screen.y,s.p1.screen.y,s.p2.screen.y,s.p2.screen.y};
        filledPolygonColor(rend,lxs,lys,4,s.colours[3]);
    }
}

void render(SDL_Renderer* rend){

    SDL_SetRenderDrawColor(rend, 0, 0, 255, 255);
    SDL_RenderClear(rend);

    segment first_segment = find_segment(position);
    float first_percent = percentRemaining((int)position,SEGMENT_LEN);
    float playerY = interpolate(first_segment.p1.world.y,first_segment.p2.world.y,first_percent);

    printf("firstsegmetn: %d\n",first_segment.index);
    float dx = -(first_segment.c * first_percent);
    float x = 0;

    float maxy = WINDOW_HEIGHT;
    int i;
    for(i=0;i<DRAW_DISTANCE;i++){
        segment s = road_segments[(first_segment.index+i)%road_len];
       // printf("drawing segment %d at y%f\n",s.index,s.p2.world.y);

        project_point(&s.p1,(0*ROAD_WIDTH)-x,CAM_HEIGHT+playerY,position - (s.index<first_segment.index? trackLen:0));
        project_point(&s.p2,(0*ROAD_WIDTH)-x-dx,CAM_HEIGHT+playerY,position - (s.index<first_segment.index? trackLen:0));

        x+=dx;
        dx+=s.c;

        if (!((s.p1.camera.z <= cam_depth) ||(s.p2.screen.y >= maxy))){

            boxColor(rend,0, s.p2.screen.y, WINDOW_WIDTH,s.p1.screen.y,s.colours[0]);
            Sint16 xs[4] = {s.p1.screen.x -s.p1.screen.z ,s.p1.screen.x+s.p1.screen.z ,
                         s.p2.screen.x+s.p2.screen.z ,s.p2.screen.x - s.p2.screen.z };
            Sint16 ys[4] = {s.p1.screen.y,s.p1.screen.y,
                         s.p2.screen.y,s.p2.screen.y};
            filledPolygonColor(rend,xs,ys,4,s.colours[1]);
            
            render_rumble(s,rend);
            render_lanes(s,rend);
            
            maxy = s.p2.screen.y;
        }         

    }
    //printf("alllsegdrawn\n");
    //draw background sky
    SDL_RenderPresent(rend);
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

    SDL_Window * window = SDL_CreateWindow(
        "window",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );

    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Rect square = {216, 96, 34, 64}; 

    //reset_road();

    cam_depth = 1 / ( tanf((FOV/2)/(180/M_PI)) );
    addSCurves();
    int running = 1;
    SDL_Event event;
    while (running) { 
        Uint64 start = SDL_GetPerformanceCounter();
        // Process input
        if (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    // End the loop if the programs is being closed
                    running = 0;
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    // Connect a controller when it is connected
                    SDL_GameControllerOpen(event.cdevice.which);
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                        // Close the program if start is pressed
                        running = 0;
                    }
                    break;
            }
        }

        // Clear the screen
        render(renderer);
        
        position+=SPEED;
        while(position>=trackLen){
            position-=trackLen;
        }
        while(position<0){
            position += trackLen;
        }
        


        Uint64 end = SDL_GetPerformanceCounter();

	    float elapsedMS = (end - start) / (float)SDL_GetPerformanceFrequency() * 1000.0f;

        SDL_Delay(floor(16.666f - elapsedMS));

    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
