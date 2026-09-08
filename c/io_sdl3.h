
#include <SDL3/SDL.h>

SDL_Cursor*CURSORS[8];

// keyboard keys

#define KEY_UP           SDLK_UP
#define KEY_DOWN         SDLK_DOWN
#define KEY_LEFT         SDLK_LEFT
#define KEY_RIGHT        SDLK_RIGHT
#define KEY_PAGEUP       SDLK_PAGEUP
#define KEY_PAGEDOWN     SDLK_PAGEDOWN
#define KEY_HOME         SDLK_HOME
#define KEY_END          SDLK_END
#define KEY_SPACE        SDLK_SPACE
#define KEY_BACKSPACE    SDLK_BACKSPACE
#define KEY_DELETE       SDLK_DELETE
#define KEY_ESCAPE       SDLK_ESCAPE
#define KEY_RETURN       SDLK_RETURN
#define KEY_TAB          SDLK_TAB
#define KEY_CAPSLOCK     SDLK_CAPSLOCK
#define KEY_LSHIFT       SDLK_LSHIFT
#define KEY_RSHIFT       SDLK_RSHIFT
#define KEY_LCTRL        SDLK_LCTRL
#define KEY_RCTRL        SDLK_RCTRL
#define KEY_LGUI         SDLK_LGUI
#define KEY_RGUI         SDLK_RGUI
#define KEY_LEFTBRACKET  SDLK_LEFTBRACKET
#define KEY_RIGHTBRACKET SDLK_RIGHTBRACKET
#define KEY_0            SDLK_0
#define KEY_1            SDLK_1
#define KEY_2            SDLK_2
#define KEY_3            SDLK_3
#define KEY_9            SDLK_9
#define KEY_l            SDLK_L
#define KEY_j            SDLK_J
#define KEY_k            SDLK_K
#define KEY_m            SDLK_M
#define KEY_r            SDLK_R
#define KEY_t            SDLK_T
#define KEY_u            SDLK_U
#define KEY_y            SDLK_Y
#define KEY_F1           SDLK_F1
#define KEY_F2           SDLK_F2
#define KEY_F3           SDLK_F3
#define KEY_F4           SDLK_F4
#define KEY_F5           SDLK_F5
#define KEY_F6           SDLK_F6
#define KEY_F7           SDLK_F7
#define KEY_F8           SDLK_F8
#define KEY_F9           SDLK_F9
#define KEY_F10          SDLK_F10
#define KEY_F11          SDLK_F11
#define KEY_F12          SDLK_F12
#define KEYPAD_UP        SDLK_KP_8
#define KEYPAD_DOWN      SDLK_KP_2
#define KEYPAD_LEFT      SDLK_KP_4
#define KEYPAD_RIGHT     SDLK_KP_6
#define KEYPAD_OK        SDLK_KP_1
#define KEYPAD_CANCEL    SDLK_KP_3

#define KEYM_LSHIFT      SDL_KMOD_LSHIFT
#define KEYM_RSHIFT      SDL_KMOD_RSHIFT
#define KEYM_LCTRL       SDL_KMOD_LCTRL
#define KEYM_RCTRL       SDL_KMOD_RCTRL
#define KEYM_LALT        SDL_KMOD_LALT
#define KEYM_RALT        SDL_KMOD_RALT
#define KEYM_LGUI        SDL_KMOD_LGUI
#define KEYM_RGUI        SDL_KMOD_RGUI

// global interpreter lock

SDL_Mutex*gil=NULL;
void interpreter_lock(void){SDL_LockMutex(gil);}
void interpreter_unlock(void){SDL_UnlockMutex(gil);}

// resources

void base_path(char*path){
	const char*t=SDL_GetBasePath();
	if(t){snprintf(path,PATH_MAX,"%s",t);}else{path[0]='\0';}
}
void open_url(char*x){
	int e=SDL_OpenURL(x);
	if(!e)printf("open url error: %s\n",SDL_GetError());
}

// clipboard

lv* get_clip(void){char*t=SDL_GetClipboardText();lv*r=lmutf8(t);SDL_free(t);return r;}
void set_clip(lv*x){SDL_SetClipboardText(drom_to_utf8(x)->sv);}

// audio

#define SFX_INPUT_FORMAT SDL_AUDIO_S8
#define SFX_OUTPUT_FORMAT SDL_AUDIO_S16
#define SFX_CHANNELS 1
int nosound=0;
SDL_AudioStream*audio_out=NULL;
SDL_AudioStream*audio_in=NULL;

void sfx_pump(void*user,Uint8*stream,int len);
void record_pump(void* userdata,Uint8* stream,int len);

void sfx_stream_pump(void*userdata,SDL_AudioStream*stream,int additional,int total){
	(void)total;if(additional<=0)return;
	Uint8*buf=malloc(additional);
	sfx_pump(userdata,buf,additional);
	SDL_PutAudioStreamData(stream,buf,additional);
	free(buf);
}
void record_stream_pump(void*userdata,SDL_AudioStream*stream,int additional,int total){
	(void)total;if(additional<=0)return;
	Uint8*buf=malloc(additional);
	int got=SDL_GetAudioStreamData(stream,buf,additional);
	if(got>0)record_pump(userdata,buf,got);
	free(buf);
}

lv*readwav(char*name,int rawsamples){
	Uint8* raw; Uint32 length; SDL_AudioSpec spec;
	if(!SDL_LoadWAV(name,&spec,&raw,&length))return sound_make(lms(0));
	SDL_AudioSpec dst={SFX_INPUT_FORMAT,SFX_CHANNELS,SFX_RATE};
	if(spec.format!=dst.format||spec.channels!=dst.channels||spec.freq!=dst.freq){
		Uint8*cvt=NULL;int cvtlen=0;
		if(SDL_ConvertAudioSamples(&spec,raw,length,&dst,&cvt,&cvtlen)){
			SDL_free(raw);raw=cvt;length=cvtlen;
		}
	}
	if(rawsamples){lv*r=lmv(1);r->c=length;r->sv=(char*)raw;return array_make(length,1,0,r);}
	lv*r=lmv(1);r->c=MIN(length,10*SFX_RATE);r->sv=(char*)raw;return sound_make(r);
}

int record_possible(void){int n=0;SDL_AudioDeviceID*d=SDL_GetAudioRecordingDevices(&n);SDL_free(d);return n>=1;}
int record_begin(void){
	SDL_AudioSpec spec={SFX_INPUT_FORMAT,SFX_CHANNELS,SFX_RATE};
	audio_in=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING,&spec,record_stream_pump,NULL);
	return audio_in!=NULL;
}
void record_pause(int device){(void)device;if(audio_in)SDL_ResumeAudioStreamDevice(audio_in);}
void record_finish(int device){(void)device;if(audio_in)SDL_PauseAudioStreamDevice(audio_in);}

// input events

void event_quit(void);
void event_touch(void);
void event_key(int c,int m,int down,const char*name);
void event_scroll(pair s);
void event_pointer_move(pair raw,pair scaled);
void event_pointer_button(int primary,int middle,int down);
void event_file(char*p);
void field_input(char*text);
void event_padbutton(int b,int down);
void event_padaxes(int x,int y);
SDL_Gamepad*pad=NULL;

void event_padbutton_wrap(int b,int down){
	int btn=b==SDL_GAMEPAD_BUTTON_DPAD_UP   ?GAMEPAD_UP:
	        b==SDL_GAMEPAD_BUTTON_DPAD_DOWN ?GAMEPAD_DN:
	        b==SDL_GAMEPAD_BUTTON_DPAD_LEFT ?GAMEPAD_LF:
	        b==SDL_GAMEPAD_BUTTON_DPAD_RIGHT?GAMEPAD_RT:
	        b==SDL_GAMEPAD_BUTTON_SOUTH     ?GAMEPAD_A2:
	        b==SDL_GAMEPAD_BUTTON_EAST      ?GAMEPAD_A1:
	        b==SDL_GAMEPAD_BUTTON_WEST      ?GAMEPAD_A2:
	        b==SDL_GAMEPAD_BUTTON_NORTH     ?GAMEPAD_A1:
	        0;
	if(btn)event_padbutton(btn,down);
}

void process_events(pair disp,pair size,int scale){
	SDL_Event e;
	while(SDL_WaitEvent(&e)){
		if(e.type==SDL_EVENT_QUIT       )event_quit();
		if(e.type==SDL_EVENT_USER       )break;
		if(e.type==SDL_EVENT_TEXT_INPUT )field_input(lmutf8((char*)e.text.text)->sv);
		if(e.type==SDL_EVENT_KEY_DOWN   )event_key(e.key.key,e.key.mod,1,SDL_GetKeyName(e.key.key));
		if(e.type==SDL_EVENT_KEY_UP     )event_key(e.key.key,e.key.mod,0,SDL_GetKeyName(e.key.key));
		if(e.type==SDL_EVENT_MOUSE_MOTION){
			pair b={(disp.x-(size.x*scale))/2,(disp.y-(size.y*scale))/2};
			pair raw={(int)e.motion.x,(int)e.motion.y};
			event_pointer_move(raw,(pair){(raw.x-b.x)/scale,(raw.y-b.y)/scale});
		}
		if(e.type==SDL_EVENT_MOUSE_WHEEL     )event_scroll((pair){e.wheel.integer_x,e.wheel.integer_y});
		if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN)event_pointer_button(e.button.button==SDL_BUTTON_LEFT,e.button.button==SDL_BUTTON_MIDDLE,1);
		if(e.type==SDL_EVENT_MOUSE_BUTTON_UP  )event_pointer_button(e.button.button==SDL_BUTTON_LEFT,e.button.button==SDL_BUTTON_MIDDLE,0);
		if(e.type==SDL_EVENT_FINGER_DOWN      )event_touch();
		if(e.type==SDL_EVENT_DROP_FILE        )event_file((char*)e.drop.data);
		if(e.type==SDL_EVENT_GAMEPAD_ADDED  &&pad==NULL){pad=SDL_OpenGamepad(e.gdevice.which);}
		if(e.type==SDL_EVENT_GAMEPAD_REMOVED&&pad!=NULL){if(SDL_GetGamepadID(pad)==e.gdevice.which){SDL_CloseGamepad(pad),pad=NULL;}}
		if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN&&pad!=NULL)event_padbutton_wrap(e.gbutton.button,1);
		if(e.type==SDL_EVENT_GAMEPAD_BUTTON_UP  &&pad!=NULL)event_padbutton_wrap(e.gbutton.button,0);
		if(e.type==SDL_EVENT_GAMEPAD_AXIS_MOTION&&pad!=NULL)event_padaxes(SDL_GetGamepadAxis(pad,0),SDL_GetGamepadAxis(pad,1));
	}
	SDL_FlushEvent(SDL_EVENT_USER);
}

Uint32 tick_pump(void*param,SDL_TimerID id,Uint32 interval){
	(void)id;
	SDL_Event e; SDL_UserEvent u;
	u.type=SDL_EVENT_USER;
	u.data1=param;
	e.user=u;
	SDL_PushEvent(&e);
	return interval;
}

// rendering

SDL_Renderer*ren;
SDL_Texture*gfx;
SDL_Texture*gtool;
#include <SDL3_image/SDL_image.h>
lv* readimage(char*path,int grayscale){
	SDL_Surface*b=IMG_Load(path);if(b==NULL)return image_empty();lv*i=lmbuff((pair){b->w,b->h});
	SDL_Surface*c=SDL_ConvertSurface(b,SDL_PIXELFORMAT_RGBA8888);
	const SDL_PixelFormatDetails*fmt=SDL_GetPixelFormatDetails(c->format);
	for(int y=0;y<b->h;y++)for(int x=0;x<b->w;x++){
		Uint32 v=((Uint32*)c->pixels)[x+(y*c->pitch/4)];Uint8 cr,cg,cb,ca;
		SDL_GetRGBA(v,fmt,NULL,&cr,&cg,&cb,&ca),i->sv[x+y*b->w]=(ca!=0xFF)?(grayscale?0xFF:0x00):readcolor(cr,cg,cb,grayscale);
	}SDL_DestroySurface(c),SDL_DestroySurface(b);return image_make(i);
}
void framebuffer_alloc(pair size,int minscale){
	(void)minscale;
	gfx=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,size.x,size.y);
}
int framebuffer_flip(pair disp,pair size,int scale,int mask,int frame,char*pal,lv*buffer){
	int* p, pitch;
	SDL_LockTexture(gfx,NULL,(void**)&p,&pitch);
	draw_frame(pal,buffer,p,pitch,frame,mask);frame_count++;
	SDL_UnlockTexture(gfx);
	SDL_FRect src={0,0,(float)size.x,(float)size.y};
	SDL_FRect dst={(disp.x-scale*size.x)/2.0f,(disp.y-scale*size.y)/2.0f,(float)(scale*size.x),(float)(scale*size.y)};
	SDL_SetRenderDrawColor(ren,0x00,0x00,0x00,0xFF);
	SDL_RenderClear(ren);
	SDL_RenderTexture(ren,gfx,&src,&dst);
	return 1;
}

void toolbar_flip(lv*buffer,int frame,char*pal,rect dest){
	pair tsize=buff_size(buffer);
	SDL_FRect src={0,0,(float)tsize.x,(float)tsize.y},dst={(float)dest.x,(float)dest.y,(float)dest.w,(float)dest.h};
	if(!gtool){pair s=buff_size(buffer);gtool=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,s.x,s.y);}
	int*p, pitch;
	SDL_LockTexture(gtool,NULL,(void**)&p,&pitch);
	draw_frame(pal,buffer,p,pitch,frame,0);
	SDL_UnlockTexture(gtool);
	SDL_RenderTexture(ren,gtool,&src,&dst);
}
void finish_flip(void){SDL_RenderPresent(ren);}

// windows

SDL_Window*win;
pair get_display_size(void){
	const SDL_DisplayMode*dis=SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	return dis?(pair){dis->w,dis->h}:(pair){0,0};
}
int get_display_density(pair disp){
	pair disp_pixels={0,0};SDL_GetWindowSizeInPixels(win,&disp_pixels.x,&disp_pixels.y);
	if(disp.x&&disp_pixels.x>disp.x)return disp_pixels.x/disp.x;
	return 1;
}
void window_set_title(char*x){SDL_SetWindowTitle(win,x);}
void window_set_opacity(float x){SDL_SetWindowOpacity(win,x);}
void window_set_cursor(int x){SDL_SetCursor(CURSORS[x]);}
void window_set_fullscreen(int full){SDL_SetWindowFullscreen(win,full);}
pair window_get_size(void){pair r={0,0};SDL_GetWindowSize(win,&r.x,&r.y);return r;}
void window_set_size(pair wsize,pair size,int scale){
	if(win){SDL_SetWindowSize(win,wsize.x,wsize.y),SDL_DestroyTexture(gfx);}
	else{
		win=SDL_CreateWindow("Decker",
			size.x*scale,size.y*scale,
			SDL_WINDOW_HIGH_PIXEL_DENSITY
		);
		ren=SDL_CreateRenderer(win,SDL_SOFTWARE_RENDERER);
		SDL_StartTextInput(win);
	}
	framebuffer_alloc(size,scale);
	SDL_SetWindowPosition(win,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
}

// entrypoint

void tick(lv*env);
void sync(void);

void io_init(void){
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | (nosound?0:SDL_INIT_AUDIO));
	gil=SDL_CreateMutex();
	CURSORS[0]=SDL_GetDefaultCursor();
	CURSORS[1]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
	CURSORS[2]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
	CURSORS[3]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
	CURSORS[4]=SDL_CreateCursor(calloc(8,sizeof(char)),calloc(8,sizeof(char)),8,8,0,0);
	CURSORS[5]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	CURSORS[6]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
	CURSORS[7]=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
	if(!nosound){
		SDL_AudioSpec spec={SFX_OUTPUT_FORMAT,SFX_CHANNELS,SFX_RATE};
		audio_out=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,sfx_stream_pump,NULL);
		SDL_ResumeAudioStreamDevice(audio_out);
	}
	SDL_AddTimer((1000/60),tick_pump,NULL);
	SDL_SetGamepadEventsEnabled(true);
}
void io_run(lv*env){
	while(!should_exit){tick(env);sync();}
	SDL_Quit();
}
