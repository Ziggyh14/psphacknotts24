#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_image.h>
#define PALETTE_SIZE 4
#define SEGMENT_LEN 200
#define RUMBLE_LEN 3
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define DRAW_DISTANCE 200
#define ROAD_WIDTH 2000
#define CAM_HEIGHT 1000
#define FOV 100

#define CHUNK_SIZE 100
#define BUFFER_CHUNKS 5

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

#define DIR_LEFT -1
#define DIR_RIGHT 1

#define ACCEL 1
#define BRAKE -1
#define DECEL 0

#define CENTRIFUGAL 0.3

#define easeIn(a,b,p) (a+(b-a)*powf(p,2))
#define easeOut(a,b,p) (a+(b-a)*(1-powf(1-p,2)))
#define easeInOut(a,b,p) (float)((float)a+(b-a)*((float)(-cosf(p*M_PI)/2)+0.5))
#define percentRemaining(n, total) ((float)(n%total)/total)
#define interpolate(a,b,p) (a+(b-a)*p)

int colours_DARK[PALETTE_SIZE] = {0xff2fa31d,0xff667064,0xffffffff,0xffffffff};
int colours_LIGHT[PALETTE_SIZE] = {0xff3edb25,0xff667064,0xff0000ff,0xff667064};
int colours_CHECKPOINT[PALETTE_SIZE] = {0xff3edb25,0xff002afa,0xff0000ff,0xff667064};

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
    float screenscale;

}point;

typedef struct sprite{
    SDL_Texture* t;
   // SDL_Rect r;
   // float speed;
    float offset;
}sprite;


typedef struct segment{

    point p1;
    point p2;
    int c;
    int index;
    int* colours;
    sprite sprites[5];
    int spritenum;
    float clip;
    float timeadded;
    int checkpoint;

}segment;


segment road_segments[CHUNK_SIZE*BUFFER_CHUNKS];
int chunk_index= 0;
int seg_index=0;
int road_len =0;
int trackLen = 0;
float position = 0;
float cam_depth;
float playerX = 0;
float playerZ = 0;
float playerW = 0;
float speed = 0;
int loops = 0;
float max_speed = (SEGMENT_LEN/0.0166666);

int A_PRESSED = 0;
int B_PRESSED = 0;
int X_PRESSED = 0;
int LEFT_PRESSED = 0;
int RIGHT_PRESSED = 0;


sprite car;

SDL_Texture* billboard;

float increase(float x,float incr, float max, float min){
    x += incr;
        while(x>=max){
            x-=max;
        }
        while(x<min){
            x += max;
        }

    return x;
}

float lastY(){
    float r;
    if(road_len == 0) return 0;
    if(chunk_index == 0&&seg_index ==0){
        return road_segments[(BUFFER_CHUNKS*CHUNK_SIZE)-1].p2.world.y;
    }
    return road_segments[(chunk_index*100)+seg_index-1].p2.world.y;
}

void addSprite(int segi,SDL_Texture* t,float offset){
    segment* s = &road_segments[segi];
    if(s->spritenum < 5){
        sprite sp;
        sp.t = t;
        sp.offset = offset;
        s->sprites[s->spritenum] = sp;
        s->spritenum = s->spritenum+1;
        printf("sprite added %d to %d\n",s->spritenum,s->index);
    }
}

void addSegment(int c,float y,int i){
    
    segment s;
    s.index = i;
    s.p1.world = zero;
    s.p1.world.y = lastY();
    s.p1.world.z = road_len*SEGMENT_LEN;
    s.p2.world = zero;
    s.p2.world.y = y;
    s.p2.world.z = (road_len+1)*SEGMENT_LEN;
    s.c = c;
    s.colours = ((road_len/RUMBLE_LEN)%2 ? colours_DARK : colours_LIGHT);
    s.spritenum = 0;
    s.checkpoint = 0;
    road_segments[i] = s;
    road_len++;
   // trackLen+=SEGMENT_LEN;
    seg_index = (seg_index+1)%CHUNK_SIZE;
}

void addChunk(float enter,float hold,float leave, int curve,float y){
    
    if(enter+hold+leave != CHUNK_SIZE){
        return;
    }
    printf("GEN CHUNK %d\n",chunk_index);
    float startY = lastY();
    printf("%d\n",chunk_index);
    float endY = startY + (y*SEGMENT_LEN);
    printf("end%d\n",chunk_index);
    float total =enter+hold+leave;
    printf("end%d\n",chunk_index);
    int i;
    printf("end%d\n",chunk_index);
    for(i = 0;i<enter;i++){
        addSegment(easeIn(0,curve,i/enter),easeInOut(startY,endY,(float)i/total),(chunk_index*100)+i);}
    printf("end%d\n",chunk_index);
    for(i = 0; i<hold;i++){
        addSegment(curve,easeInOut(startY,endY,(enter+(float)i)/total),(chunk_index*100)+i+enter);}
    printf("end%d\n",chunk_index);
    for(i = 0; i<leave;i++){
        addSegment(easeInOut(curve,0,i/leave),easeInOut(startY,endY,(enter+hold+(float)i)/total),(chunk_index*100)+i+enter+hold);}
    printf("end%d\n",chunk_index);
    chunk_index++;

    chunk_index%=5;
    trackLen+=CHUNK_SIZE * SEGMENT_LEN;
    printf("tracklen %d\n",trackLen);
    loops++;
    
    
}

void addGenRoad(){

    for(int i = 0; i<BUFFER_CHUNKS;i++){
        addChunk(LEN_MEDIUM,LEN_SHORT,LEN_SHORT,CURVE_EASY,HILL_LOW);
        printf("outside for%d\n",chunk_index);
        
    }
   // trackLen = BUFFER_CHUNKS*CHUNK_SIZE*SEGMENT_LEN;
}

void addCheckpoint(int i,float time_added){

    segment* s = &road_segments[i];
    s->colours = colours_CHECKPOINT;
    s->checkpoint = 1;
    s->timeadded = time_added;

}

segment find_segment(float z){
    return road_segments[(int)(z/SEGMENT_LEN) % (CHUNK_SIZE*BUFFER_CHUNKS)];
    
}


void project_point(point* p,int camx, int camy, int camz){

    p->camera.x = (p->world.x ) - camx;
    p->camera.y = (p->world.y ) - camy;
    p->camera.z = (p->world.z ) - camz;

    p->screenscale = cam_depth/p->camera.z;
    p->screen.x = round((WINDOW_WIDTH/2) + (p->screenscale *  p->camera.x * WINDOW_WIDTH/2));
    p->screen.y = round((WINDOW_HEIGHT/2) - (p->screenscale * p->camera.y  * WINDOW_HEIGHT/2));
   // p->screen.y = p->screen.y < 0 ? 0 : p->screen.y;
    p->screen.z = round((p->screenscale * ROAD_WIDTH * WINDOW_WIDTH/2));

}

void render_sprite(SDL_Renderer* rend, SDL_Texture* tex, float scale, float destX, float destY, float w, float h, float xOffset, float yOffset,float clip){
    
    float destW = (w*scale*(float)WINDOW_WIDTH/2) * ((0.3 * 1/270) * ROAD_WIDTH); 
    float destH = (h*scale*(float)WINDOW_WIDTH/2) * ((0.3 * 1/270) * ROAD_WIDTH); 
    destX = destX + (xOffset * destW);
    destY = destY + (yOffset * destH);

    float clipH = clip ? ( 0 > destY + destH - clip ? 0 : destY + destH - clip) : 0;
    if(clipH < destH){
        SDL_Rect r = {destX,destY,destW,destH};
        SDL_RenderCopy(rend, tex, NULL, &r);

    }
    


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

void render_background(SDL_Renderer* rend){


   // SDL_RenderCopy(rend,sky.t,NULL,&(sky.r));
   // SDL_RenderCopy(rend,hills.t,NULL,&(hills.r));
   // SDL_RenderCopy(rend,trees.t,NULL,&(trees.r));
}

void render_player(SDL_Renderer* rend){
    float bounce = (1.5 * (1/SDL_GetPerformanceCounter()) * speed/max_speed *(4/3));
    playerW = (442*(float)cam_depth/playerZ*(float)WINDOW_WIDTH/2) * ((0.3 * 1/270) * ROAD_WIDTH);
    render_sprite(rend,car.t,(float)cam_depth/playerZ,WINDOW_WIDTH/2,WINDOW_HEIGHT,442,304,-0.5,-1,0);
}

void render(SDL_Renderer* rend){

    SDL_SetRenderDrawColor(rend, 0, 0, 255, 255);
    SDL_RenderClear(rend);

   // render_background(rend);

    segment first_segment = find_segment(position);
    float first_percent = percentRemaining((int)position,SEGMENT_LEN);
    float playerY = interpolate(first_segment.p1.world.y,first_segment.p2.world.y,first_percent);

    float dx = -(first_segment.c * first_percent);
    float x = 0;

    float maxy = WINDOW_HEIGHT;
    int i;
    //printf("first segment index %d\n",first_segment.index);
    for(i=0;i<DRAW_DISTANCE;i++){
        segment* s = &road_segments[(first_segment.index+i)%(BUFFER_CHUNKS*CHUNK_SIZE)];
         
       // printf("drawing segment %d at world z%f\n",s->index,s->p2.world.z);

        project_point(&s->p1,(playerX*ROAD_WIDTH)-x,CAM_HEIGHT+playerY,position - (s->index<first_segment.index? 0:0));
        project_point(&s->p2,(playerX*ROAD_WIDTH)-x-dx,CAM_HEIGHT+playerY,position - (s->index<first_segment.index? 0:0));

        x+=dx;
        dx+=s->c;

        if (!((s->p1.camera.z <= cam_depth) ||(s->p2.screen.y >= maxy))){

            boxColor(rend,0, s->p2.screen.y, WINDOW_WIDTH,s->p1.screen.y,s->colours[0]);
            Sint16 xs[4] = {s->p1.screen.x -s->p1.screen.z ,s->p1.screen.x+s->p1.screen.z ,
                         s->p2.screen.x+s->p2.screen.z ,s->p2.screen.x - s->p2.screen.z };
            Sint16 ys[4] = {s->p1.screen.y,s->p1.screen.y,
                         s->p2.screen.y,s->p2.screen.y};
            filledPolygonColor(rend,xs,ys,4,s->colours[1]);
            
            render_rumble(*s,rend);
            render_lanes(*s,rend);
            
            maxy = s->p2.screen.y;
            s->clip = maxy;
        }       

    }

   


    for(i = (DRAW_DISTANCE-1) ; i > 0; i--){
        segment* s = &road_segments[(first_segment.index+i)%(BUFFER_CHUNKS*CHUNK_SIZE)];
      
        int j;
        for(j=0;j<s->spritenum; j++){
            float sx = 0,sy = 0;
            float spritescale = cam_depth/(float)s->p1.camera.z;

            //printf("sx: %f, sy: %f, scale: %F, offset %f\n",s.p1.screen.x,s.p1.screen.y,spritescale,s.sprites[j].offset);

            sx = s->p1.screen.x + ((spritescale) * (s->sprites[j].offset) * ROAD_WIDTH * WINDOW_WIDTH/2);
            sy = s->p1.screen.y;
            int sw = 0,sh = 0;
            SDL_QueryTexture(s->sprites[j].t, NULL, NULL, &sw, &sh);
           
            render_sprite(rend,s->sprites[j].t,spritescale,sx,sy,(float)sw,(float)sh,(s->sprites[j].offset < 0 ? -1 : 0),-1,s->clip);
            
        }
    }

    render_player(rend);
    //printf("alllsegdrawn\n");
    //draw background sky
    SDL_RenderPresent(rend);
}

int overlap(x1,w1,x2,w2,p){

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
    car.t = IMG_LoadTexture(renderer,"res/car.png");
    billboard = IMG_LoadTexture(renderer, "res/billboard.png");


    SDL_Rect square = {216, 96, 34, 64}; 
    SDL_Rect r; r.x = 0; r.y = 0; r.w = WINDOW_WIDTH; r.h = WINDOW_HEIGHT/2;
    
   
    //reset_road();

    cam_depth = 1 / ( tanf((FOV/2)/(180/M_PI)) );
    playerZ = (float)CAM_HEIGHT*cam_depth;
    addGenRoad();
    addCheckpoint(100,100);
   // printf("roads gened\n");
    addSprite(5,  billboard, -1);
    addSprite(60,  billboard, -1);
    addSprite(60,  billboard, 1);
    addSprite(300,  billboard, -1);
    int running = 1;
    int dir = 0;
    int acc = 0;
    float drift_tightness = 0;
    float drift_factor = 0;
    float drift_angle = 0;
    float time = 0;
    Uint64 NOW = SDL_GetPerformanceCounter();
    Uint64 LAST = 0;
    float dt = 0;
    printf("max speed %f\n",max_speed);

    SDL_Event event;
    while (running) { 

        LAST = NOW;
        NOW = SDL_GetPerformanceCounter();

        dt = (float)((NOW - LAST) * 1000/(double)SDL_GetPerformanceFrequency());
        dt*=0.001;
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
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                        LEFT_PRESSED = 1;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                        RIGHT_PRESSED = 1;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        A_PRESSED = 1;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                        B_PRESSED = 1;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_X) {
                        X_PRESSED = 1;
                    }
                    break;
                case SDL_CONTROLLERBUTTONUP:
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                        LEFT_PRESSED = 0;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                        RIGHT_PRESSED = 0;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        A_PRESSED = 0;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                        B_PRESSED = 0;
                    }
                    if(event.cbutton.button == SDL_CONTROLLER_BUTTON_X) {
                        X_PRESSED = 0;
                    }
                    break;
            }
        }

        segment playerSegment = find_segment(position+playerZ);
        position = increase(position,dt*speed,INFINITY,0);

        if(playerSegment.checkpoint){
            time+=playerSegment.timeadded;
            road_segments[playerSegment.index].checkpoint = 0;
        }
        //printf("time: %f\n",time);

        

        float dx = dt * 2 * (speed/max_speed);
        
        
        if(LEFT_PRESSED){
                playerX = playerX  -  dx;
        }
        if(RIGHT_PRESSED){
            playerX = playerX  +  dx;
        }
        
        if(X_PRESSED){
            speed = speed + ((-max_speed)*dt);
        }else if(B_PRESSED){
            speed = speed + ((-max_speed/2)*dt);
        }
        else if(A_PRESSED){
            speed = speed + ((max_speed /5)*dt);
        }
        else{
            speed = speed + ((-max_speed/5)*dt);
        }


        playerX = playerX - (dx * (speed/max_speed) * playerSegment.c * CENTRIFUGAL);
       
        speed = speed > max_speed ? max_speed : speed;
        speed = speed < 0 ? 0 : speed;
        //speed *= drift_tightness

        if (((playerX < -1) || (playerX > 1)) && (speed > max_speed/4))
            speed = speed + (-max_speed/2* dt);

        if ((playerX < -1) || (playerX > 1)){
            for(int n = 0 ; n < playerSegment.spritenum; n++) {
                sprite s  = playerSegment.sprites[n];
                int sw,sh;
                SDL_QueryTexture(s.t,NULL,NULL,&sw,&sh);
                if (overlap(playerX,playerW,s.offset+sw/2*(s.offset>0?1:-1),sw)) {
                    //hitjkfhewgjn
                }   
            }

        }

        segment firstSegment = find_segment(position);
        printf("firstsegment %d\n",firstSegment.index);
        if(((chunk_index)*100)+100<firstSegment.index-101){
            printf("chunk index%d\n",chunk_index);
            addChunk(LEN_SHORT,LEN_SHORT,LEN_MEDIUM,CURVE_MEDIUM,HILL_LOW);
            printf("gen chunk at index %d\n",chunk_index-1);
        }

        render(renderer);


      //  Uint64 END = SDL_GetPerformanceCounter();

	   // float elapsedMS = (END - NOW) / (float)SDL_GetPerformanceFrequency() * 1000.0f;

       // SDL_Delay(floor(16.666f - elapsedMS));

    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
