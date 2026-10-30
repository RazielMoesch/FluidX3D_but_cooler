#include "graphics.hpp"

vector<string> main_arguments = vector<string>(); // console arguments
std::atomic_bool running = true;

#ifdef GRAPHICS
Camera camera;

// reserved keys for graphics: W,A,S,D, I,J,K,L, F, R,U, V,B, C,VK_SPACE, Y,X, N,M
//bool key_A=false, key_B=false, key_C=false, key_D=false, key_E=false, key_F=false, key_G=false, key_H=false, key_I=false, key_J=false, key_K=false, key_L=false, key_M=false;
//bool key_N=false, key_O=false, key_P=false, key_Q=false, key_R=false, key_S=false, key_T=false, key_U=false, key_V=false, key_W=false, key_X=false, key_Y=false, key_Z=false;
bool key_E=false, key_G=false, key_H=false, key_O=false, key_P=false, key_Q=false, key_T=false, key_Z=false;
bool key_1=false, key_2=false, key_3=false, key_4=false, key_5=false, key_6=false, key_7=false, key_8=false, key_9=false, key_0=false;

const uint light_sources_N = 100u; // maximal number of light sources
float3 light_sources[light_sources_N]; // coordinates of light sources
uint light_sources_n = 0u; // number of light sources

bool convert(int& rx, int& ry, float& rz, const float3& p, const int stereo) { // 3D -> 2D
	float3 t, r;
	t.x = p.x-(camera.free ? camera.pos.x : 0.0f)-(float)stereo*camera.eye_distance/camera.zoom*camera.R.xx; // transformation
	t.y = p.y-(camera.free ? camera.pos.y : 0.0f)-(float)stereo*camera.eye_distance/camera.zoom*camera.R.xy;
	t.z = p.z-(camera.free ? camera.pos.z : 0.0f);
	r.z = camera.R.zx*t.x+camera.R.zy*t.y+camera.R.zz*t.z; // z-position for z-buffer
	const float rs = camera.zoom*camera.dis/(camera.dis-r.z*camera.zoom); // perspective (reciprocal is more efficient)
	if(rs<=0.0f) return false; // point is behins camera
	const float tv = camera.tv&&stereo!=0 ? 0.5f : 1.0f;
	r.x = ((camera.R.xx*t.x+camera.R.xy*t.y+camera.R.xz*t.z)*rs+(float)stereo*camera.eye_distance)*tv+(0.5f+(float)stereo*0.25f)*(float)camera.width; // x position on screen
	r.y =  (camera.R.yx*t.x+camera.R.yy*t.y+camera.R.yz*t.z)*rs+0.5f*(float)camera.height; // y position on screen
	rx = (int)(r.x+0.5f);
	ry = (int)(r.y+0.5f);
	rz = r.z;
	return true;
}

bool is_off_screen(const int x, const int y) { // check if point is off screen
	return x<0||x>=(int)camera.width  ||y<0||y>=(int)camera.height; // entire screen
}

bool is_off_screen(const int x, const int y, const int stereo) { // check if point is off screen
	switch(stereo) {
		default: return x<                  0||x>=(int)camera.width  ||y<0||y>=(int)camera.height; // entire screen
		case -1: return x<                  0||x>=(int)camera.width/2||y<0||y>=(int)camera.height; // left half
		case +1: return x<(int)camera.width/2||x>=(int)camera.width  ||y<0||y>=(int)camera.height; // right half
	}
}

bool intersect_lines(const int x0, const int y0, const int x1, const int y1, const int xA, const int yA, const int xB, const int yB) { // check if two lines intersect
	const float d = (float)((yB-yA)*(x1-x0)-(xB-xA)*(y1-y0));
	if(d==0.0f) return false; // lines are parallel
	const float ua = ((xB-xA)*(y0-yA)-(yB-yA)*(x0-xA))/d;
	const float ub = ((x1-x0)*(y0-yA)-(y1-y0)*(x0-xA))/d;
	return ua>=0.0f && ua<=1.0f && ub>=0.0f && ub<=1.0f;
}

bool intersect_line_rectangle(const int x0, const int y0, const int x1, const int y1, const int xA, const int yA, const int xB, const int yB) { // check if line intersects rectangle
	return intersect_lines(x0, y0, x1, y1, xA, yA, xB, yA) || intersect_lines(x0, y0, x1, y1, xA, yB, xB, yB) || intersect_lines(x0, y0, x1, y1, xA, yA, xA, yB) || intersect_lines(x0, y0, x1, y1, xB, yA, xB, yB);
}

bool intersects_screen(const int x0, const int y0, const int x1, const int y1, const int stereo) {
	switch(stereo) {
		case  0: return intersect_line_rectangle(x0, y0, x1, y1,              0, 0, camera.width  , camera.height);
		case -1: return intersect_line_rectangle(x0, y0, x1, y1,              0, 0, camera.width/2, camera.height);
		case +1: return intersect_line_rectangle(x0, y0, x1, y1, camera.width/2, 0, camera.width  , camera.height);
	}
	return false;
}

void set_light(const uint i, const float3& position) {
	if(i<light_sources_N) {
		light_sources[i] = position;
		light_sources_n = max(light_sources_n, i+1u);
	}
}

int shading(const int color, const float3& p, const float3& normal, const bool translucent=false) {
	const float nl2 = sq(normal.x)+sq(normal.y)+sq(normal.z); // only one sqrt instead of two
	float br = 0.0f;
	for(uint i=0u; i<light_sources_n; i++) {
		const float3 d = light_sources[i]-p; // direction of light source
		const float dl2 = sq(d.x)+sq(d.y)+sq(d.z);
		const float bri = dot(d, normal)/sqrt(nl2*dl2);
		br = fmax(br, translucent ? fabs(bri) : bri);
	}
	return color_mul(color, fmax(1.25f*br, 0.3f));
}

ulong get_font_pixels(const int character) {
	const ulong pixels[224] = { // font data (my own 6x11 monospace font)
		0x0000000000000000ull, 0x2082082080082000ull, 0x5145000000000000ull, 0x514F94514F945000ull, 0x21CAA870AA9C2000ull, 0x4AA50421052A9000ull, 0x2145085628A27400ull, 0x2082000000000000ull,
		0x0842082082040800ull, 0x4081041041084000ull, 0xA9C72A0000000000ull, 0x000208F882000000ull, 0x0000000000041080ull, 0x000000F800000000ull, 0x00000000000C3000ull, 0x0821042104208000ull,
		0x7228A28A28A27000ull, 0x0862928820820800ull, 0x722882108420F800ull, 0x7228823028A27000ull, 0x10C51493E1041000ull, 0xFA083C082082F000ull, 0x722820F228A27000ull, 0xF821042084104000ull,
		0x7228A27228A27000ull, 0x7228A27820A27000ull, 0x00030C00030C0000ull, 0x00030C00030C1080ull, 0x0021084081020000ull, 0x00003E03E0000000ull, 0x0204081084200000ull, 0x7228842080082000ull,
		0x7A1B75D75D6E81E0ull, 0x20851453E8A28800ull, 0xF228A2F228A2F000ull, 0x7228208208227000ull, 0xF228A28A28A2F000ull, 0xFA0820F20820F800ull, 0xFA0820F208208000ull, 0x722820B228A27000ull,
		0x8A28A2FA28A28800ull, 0xF88208208208F800ull, 0xF820820820A27000ull, 0x8A4928C289248800ull, 0x820820820820F800ull, 0x8B6DAAAA28A28800ull, 0x8B2CAAAAA9A68800ull, 0xFA28A28A28A2F800ull,
		0xF228A2F208208000ull, 0x7228A28A28A46800ull, 0xF228A2F289228800ull, 0x7228207020A27000ull, 0xF882082082082000ull, 0x8A28A28A28A27000ull, 0x8A28A25145082000ull, 0x8A28A28AAA945000ull,
		0x8A25142145228800ull, 0x8A25142082082000ull, 0xF82104210420F800ull, 0x3882082082083800ull, 0x8204102041020800ull, 0xE08208208208E000ull, 0x2148800000000000ull, 0x000000000000F800ull,
		0x2040000000000000ull, 0x0000007027A27000ull, 0x820820F228A2F000ull, 0x0000007A08207800ull, 0x0820827A28A27800ull, 0x000000722FA07000ull, 0x1882087082082000ull, 0x0000007A28A27827ull,
		0x820820F228A28800ull, 0x0002002082082000ull, 0x0002002082082284ull, 0x4104104946144800ull, 0x4104104104103000ull, 0x000000F2AAAAA800ull, 0x000000F228A28800ull, 0x0000007228A27000ull,
		0x0000007124927104ull, 0x0000007249247041ull, 0x0000007124104000ull, 0x0000003907827000ull, 0x2082087082081800ull, 0x0000008A28A27800ull, 0x0000008A25142000ull, 0x0000008AA7145000ull,
		0x0000008942148800ull, 0x0000008A25142084ull, 0x000000F84210F800ull, 0x0841042041040800ull, 0x2082082082082000ull, 0x8104102104108000ull, 0x000010A840000000ull, 0x0000000000000000ull,
		0x7228208208227084ull, 0x0005008A28A27800ull, 0x004200722FA07000ull, 0x0085007027A27000ull, 0x0005007027A27000ull, 0x0102007027A27000ull, 0x2142007027A27000ull, 0x0000007A08207884ull,
		0x008500722FA07000ull, 0x000500722FA07000ull, 0x010200722FA07000ull, 0x0005002082082000ull, 0x0085002082082000ull, 0x0102002082082000ull, 0xA8851453E8A28800ull, 0x21421453E8A28800ull,
		0x108FA083C820F800ull, 0x000000D89FE85C00ull, 0x5D4B2CFA49249C00ull, 0x0085007228A27000ull, 0x0005007228A27000ull, 0x0102007228A27000ull, 0x0085008A28A27800ull, 0x0102008A28A27800ull,
		0x0005008A25142084ull, 0x500FA28A28A2F800ull, 0x5008A28A28A27000ull, 0x002726AB27200000ull, 0x390410F10410F800ull, 0x082726AB27208000ull, 0x0008942148800000ull, 0x188208708208C000ull,
		0x0042007027A27000ull, 0x0042002082082000ull, 0x0042007228A27000ull, 0x0042008A28A27800ull, 0x42A1007124924800ull, 0x42A122CAAAA68800ull, 0x604624700F800000ull, 0x7228A2700F800000ull,
		0x2080082108A27000ull, 0x01E86DAEDAE17800ull, 0x000000F820000000ull, 0x4B25142164A4B800ull, 0x4B25142125AF8800ull, 0x2080082082082000ull, 0x0002529122400000ull, 0x0009122529000000ull,
		0x9004802409004800ull, 0xA95222568095A120ull, 0xA95A95A95A95A950ull, 0x2082082082082082ull, 0x2082082382082082ull, 0x108008514FA28800ull, 0x214008514FA28800ull, 0x408008514FA28800ull,
		0x01E867A699E17800ull, 0x514514D04D145145ull, 0x5145145145145145ull, 0x000000F04D145145ull, 0x514514D04F000000ull, 0x08472AA2A7108000ull, 0x8A251421C21C2000ull, 0x0000000382082082ull,
		0x20820820F0000000ull, 0x20820823F0000000ull, 0x00000003F2082082ull, 0x20820820F2082082ull, 0x00000003F0000000ull, 0x20820823F2082082ull, 0x42A1007027A27000ull, 0x42A108514FA28800ull,
		0x5145145D07C00000ull, 0x0000007D05D45145ull, 0x514514DC0FC00000ull, 0x000000FC0DD45145ull, 0x5145145D05D45145ull, 0x000000FC0FC00000ull, 0x514514DC0DD45145ull, 0x0227228A27220000ull,
		0xA10A047228A27000ull, 0x712492E924927000ull, 0x214FA083C820F800ull, 0x500FA083C820F800ull, 0x408FA083C820F800ull, 0x000000608208F800ull, 0x10803E208208F800ull, 0x21403E208208F800ull,
		0x01403E208208F800ull, 0x2082082380000000ull, 0x00000000F2082082ull, 0xFFFFFFFFFFFFFFF0ull, 0x00000003FFFFFFF0ull, 0x2082080002082080ull, 0x40803E208208F800ull, 0xFFFFFFFC00000000ull,
		0x10803E8A28A2F800ull, 0x3124944925104000ull, 0x21403E8A28A2F800ull, 0x40803E8A28A2F800ull, 0x42A1007228A27000ull, 0x42A13E8A28A2F800ull, 0x0000008A28A2FA08ull, 0x82082CCA28A2F208ull,
		0x000820F228A2F208ull, 0x1088A28A28A2F800ull, 0x2140228A28A2F800ull, 0x4088A28A28A2F800ull, 0x0042008A25142084ull, 0x12A8945082082000ull, 0xF800000000000000ull, 0x1080000000000000ull,
		0x0000007000000000ull, 0x00823E20803E0000ull, 0x00000000003E03E0ull, 0xC49C8AD0A3974800ull, 0x7BAEBA6820A27000ull, 0x31240C49230248C0ull, 0x000200F802000000ull, 0x0000000000002080ull,
		0x7147000000000000ull, 0x0140000000000000ull, 0x0000002000000000ull, 0x2182087000000000ull, 0x6046046000000000ull, 0x6042107000000000ull, 0x000FFFFFFFC00000ull, 0x0000000000000000ull
	};
	return pixels[clamp(character+256*(character<0)-32, 0, 223)];
}

void draw(const int x, const int y, const float z, const int color, const int stereo) {
	const int index=x+y*(int)camera.width, iz=(int)(z*(2147483647.0f/10000.0f));
	if(!is_off_screen(x, y, stereo)&&iz>camera.zbuffer[index]) {
		camera.zbuffer[index] = iz;
		camera.bitmap[index] = color; // only draw if point is on screen and first in zbuffer
	}
}

void convert_pixel(const float3& p, const int color, const int stereo) {
	int rx, ry; float rz;
	if(convert(rx, ry, rz, p, stereo) && !is_off_screen(rx, ry, stereo)) {
		draw(rx, ry, rz, color, stereo);
	}
}

void convert_circle(const float3& p, const float r, const int color, const int stereo) {
	int rx, ry; float rz;
	if(convert(rx, ry, rz, p, stereo)) {
		const float rs = camera.zoom*camera.dis/(camera.dis-rz*camera.zoom);
		const int radius = (int)(rs*r+0.5f);
		switch(stereo) {
			default: if(rx<                   -radius||rx>=(int)camera.width  +radius || ry<-radius||ry>=(int)camera.height+radius) return; break; // cancel drawing if circle is off screen
			case -1: if(rx<                   -radius||rx>=(int)camera.width/2+radius || ry<-radius||ry>=(int)camera.height+radius) return; break;
			case +1: if(rx<(int)camera.width/2-radius||rx>=(int)camera.width  +radius || ry<-radius||ry>=(int)camera.height+radius) return; break;
		}
		int d=-radius, x=radius, y=0; // Bresenham algorithm for circle
		while(x>=y) {
			draw(rx+x, ry+y, rz, color, stereo);
			draw(rx-x, ry+y, rz, color, stereo);
			draw(rx+x, ry-y, rz, color, stereo);
			draw(rx-x, ry-y, rz, color, stereo);
			draw(rx+y, ry+x, rz, color, stereo);
			draw(rx-y, ry+x, rz, color, stereo);
			draw(rx+y, ry-x, rz, color, stereo);
			draw(rx-y, ry-x, rz, color, stereo);
			d += 2*y+1;
			y++;
			if(d>0) d-=2*(--x);
		}
	}
}

void convert_line(const float3& p0, const float3& p1, const int color, const int stereo) {
	int r0x, r0y, r1x, r1y; float r0z, r1z;
	if(convert(r0x, r0y, r0z, p0, stereo) && convert(r1x, r1y, r1z, p1, stereo)
		&& !(is_off_screen(r0x, r0y, stereo) && is_off_screen(r1x, r1y, stereo) && !intersects_screen(r0x, r0y, r1x, r1y, stereo))) {
		int x=r0x, y=r0y; // Bresenham algorithm
		const float z = 0.5f*(r0z+r1z); // approximate line z position for each pixel to be equal
		const int dx= abs(r1x-r0x), sx=2*(r0x<r1x)-1;
		const int dy=-abs(r1y-r0y), sy=2*(r0y<r1y)-1;
		int err = dx+dy;
		while(x!=r1x||y!=r1y) {
			draw(x, y, z, color, stereo);
			const int e2 = 2*err;
			if(e2>dy) { err+=dy; x+=sx; }
			if(e2<dx) { err+=dx; y+=sy; }
		}
	}
}

void convert_triangle(const float3& p0, const float3& p1, const float3& p2, const int color, const int stereo) {
	int r0x, r0y, r1x, r1y, r2x, r2y; float r0z, r1z, r2z;
	if(convert(r0x, r0y, r0z, p0, stereo) && convert(r1x, r1y, r1z, p1, stereo) && convert(r2x, r2y, r2z, p2, stereo)
		&& !(is_off_screen(r0x, r0y, stereo) && is_off_screen(r1x, r1y, stereo) && is_off_screen(r2x, r2y, stereo)
		&& !intersects_screen(r0x, r0y, r1x, r1y, stereo) && !intersects_screen(r1x, r1y, r2x, r2y, stereo) && !intersects_screen(r2x, r2y, r0x, r0y, stereo))) {
		if(r0y==r1y&&r0y==r2y) return; // return for degenerate triangles
		if(r0y>r1y) { std::swap(r0x, r1x); std::swap(r0y, r1y); } // sort vertices ascending by y
		if(r0y>r2y) { std::swap(r0x, r2x); std::swap(r0y, r2y); }
		if(r1y>r2y) { std::swap(r1x, r2x); std::swap(r1y, r2y); }
		const float z = (r0z+r1z+r2z)/3.0f; // approximate triangle z position for each pixel to be equal
		for(int y=r0y; y<r1y; y++) { // Bresenham algorithm (lower triangle half)
			const int xA = r0x+(r2x-r0x)*(y-r0y)/(r2y-r0y);
			const int xB = r0x+(r1x-r0x)*(y-r0y)/(r1y-r0y);
			for(int x=min(xA, xB); x<max(xA, xB); x++) {
				draw(x, y, z, color, stereo);
			}
		}
		for(int y=r1y; y<r2y; y++) { // Bresenham algorithm (upper triangle half)
			const int xA = r0x+(r2x-r0x)*(y-r0y)/(r2y-r0y);
			const int xB = r1x+(r2x-r1x)*(y-r1y)/(r2y-r1y);
			for(int x=min(xA, xB); x<max(xA, xB); x++) {
				draw(x, y, z, color, stereo);
			}
		}
	}
}

void convert_triangle_interpolated(const float3& p0, const float3& p1, const float3& p2, int c0, int c1, int c2, const int stereo) {
	int r0x, r0y, r1x, r1y, r2x, r2y; float r0z, r1z, r2z;
	if(convert(r0x, r0y, r0z, p0, stereo) && convert(r1x, r1y, r1z, p1, stereo) && convert(r2x, r2y, r2z, p2, stereo)
		&& !(is_off_screen(r0x, r0y, stereo) && is_off_screen(r1x, r1y, stereo) && is_off_screen(r2x, r2y, stereo)
		&& !intersects_screen(r0x, r0y, r1x, r1y, stereo) && !intersects_screen(r1x, r1y, r2x, r2y, stereo) && !intersects_screen(r2x, r2y, r0x, r0y, stereo))) {
		if(r0y==r1y&&r0y==r2y) return; // return for degenerate triangles
		if(r0y>r1y) { std::swap(r0x, r1x); std::swap(r0y, r1y); std::swap(c0, c1); } // sort vertices ascending by y
		if(r0y>r2y) { std::swap(r0x, r2x); std::swap(r0y, r2y); std::swap(c0, c2); }
		if(r1y>r2y) { std::swap(r1x, r2x); std::swap(r1y, r2y); std::swap(c1, c2); }
		const float z = (r0z+r1z+r2z)/3.0f; // approximate triangle z position for each pixel to be equal
		const float d = (float)((r1y-r2y)*(r0x-r2x)+(r2x-r1x)*(r0y-r2y));
		for(int y=r0y; y<r1y; y++) { // Bresenham algorithm (lower triangle half)
			const int xA = r0x+(r2x-r0x)*(y-r0y)/(r2y-r0y);
			const int xB = r0x+(r1x-r0x)*(y-r0y)/(r1y-r0y);
			for(int x=min(xA, xB); x<max(xA, xB); x++) {
				const float w0 = clamp((float)((r1y-r2y)*(x-r2x)+(r2x-r1x)*(y-r2y))/d, 0.0f, 1.0f); // barycentric coordinates
				const float w1 = clamp((float)((r2y-r0y)*(x-r2x)+(r0x-r2x)*(y-r2y))/d, 0.0f, 1.0f);
				const float w2 = clamp(1.0f-w0-w1, 0.0f, 1.0f);
				const int color = color_mix_3(c0, c1, c2, w0, w1, w2); // interpolate color
				draw(x, y, z, color, stereo);
			}
		}
		for(int y=r1y; y<r2y; y++) { // Bresenham algorithm (upper triangle half)
			const int xA = r0x+(r2x-r0x)*(y-r0y)/(r2y-r0y);
			const int xB = r1x+(r2x-r1x)*(y-r1y)/(r2y-r1y);
			for(int x=min(xA, xB); x<max(xA, xB); x++) {
				const float w0 = clamp((float)((r1y-r2y)*(x-r2x)+(r2x-r1x)*(y-r2y))/d, 0.0f, 1.0f); // barycentric coordinates
				const float w1 = clamp((float)((r2y-r0y)*(x-r2x)+(r0x-r2x)*(y-r2y))/d, 0.0f, 1.0f);
				const float w2 = clamp(1.0f-w0-w1, 0.0f, 1.0f);
				const int color = color_mix_3(c0, c1, c2, w0, w1, w2); // interpolate color
				draw(x, y, z, color, stereo);
			}
		}
	}
}

void convert_text(const float3& p, const string& s, const float r, const int color, const int stereo) {
	int rx, ry; float rz;
	if(convert(rx, ry, rz, p, stereo)) {
		const float rs = camera.zoom*camera.dis/(camera.dis-rz*camera.zoom);
		const int radius = (int)(rs*r+0.5f);
		const float tr = fmax(0.85f*radius, 2.0f);
		rx += 4+(int)tr;
		ry += 3+(int)tr;
		for(int i=0u; i<(int)length(s); i++) {
			const int character = (int)s[i];
			const ulong pixels = get_font_pixels(character);
			for(int k=0; k<64; k++) {
				if((pixels>>(63-k))&1) draw(rx+i*6+k%6+(character==113), ry+k/6, rz, color, stereo);
			}
		}
	}
}

void draw_pixel(const int x, const int y, const int color) {
	if(!is_off_screen(x, y)) camera.bitmap[x+y*(int)camera.width] = color; // only draw if point is on screen
}
void draw_circle(const int x, const int y, const int r, const int color) {
	int d=-r, dx=r, dy=0; // Bresenham algorithm for circle
	while(dx>=dy) {
		draw_pixel(x+dx, y+dy, color);
		draw_pixel(x-dx, y+dy, color);
		draw_pixel(x+dx, y-dy, color);
		draw_pixel(x-dx, y-dy, color);
		draw_pixel(x+dy, y+dx, color);
		draw_pixel(x-dy, y+dx, color);
		draw_pixel(x+dy, y-dx, color);
		draw_pixel(x-dy, y-dx, color);
		d += 2*dy+1;
		dy++;
		if(d>0) d-=2*(--dx);
	}
}
void draw_line(const int x0, const int y0, const int x1, const int y1, const int color) {
	int x=x0, y=y0; // Bresenham algorithm
	const int dx= abs(x1-x0), sx=2*(x0<x1)-1;
	const int dy=-abs(y1-y0), sy=2*(y0<y1)-1;
	int err = dx+dy;
	while(x!=x1||y!=y1) {
		draw_pixel(x, y, color);
		const int e2 = 2*err;
		if(e2>dy) { err+=dy; x+=sx; }
		if(e2<dx) { err+=dx; y+=sy; }
	}
}
void draw_triangle(const int x0, const int y0, const int x1, const int y1, const int x2, const int y2, const int color) {
	int r0x=x0, r0y=y0, r1x=x1, r1y=y1, r2x=x2, r2y=y2;
	if(r0y==r1y&&r0y==r2y) return; // return for degenerate triangles
	if(r0y>r1y) { std::swap(r0x, r1x); std::swap(r0y, r1y); } // sort vertices ascending by y
	if(r0y>r2y) { std::swap(r0x, r2x); std::swap(r0y, r2y); }
	if(r1y>r2y) { std::swap(r1x, r2x); std::swap(r1y, r2y); }
	for(int y=r0y; y<r1y; y++) { // Bresenham algorithm (lower triangle half)
		const int xA = r0x+(r2x-r0x)*(y-r0y)/(r2y-r0y);
		const int xB = r0x+(r1x-r0x)*(y-r0y)/(r1y-r0y);
		for(int x=min(xA, xB); x<max(xA, xB); x++) {
			draw_pixel(x, y, color);
		}
	}
	for(int y=r1y; y<r2y; y++) { // Bresenham algorithm (upper triangle half)
		const int xA = r0x+(r2x-r0x)*(y-r0y)/(r2y-r0y);
		const int xB = r1x+(r2x-r1x)*(y-r1y)/(r2y-r1y);
		for(int x=min(xA, xB); x<max(xA, xB); x++) {
			draw_pixel(x, y, color);
		}
	}
}
void draw_rectangle(const int x0, const int y0, const int x1, const int y1, const int color) {
	for(int dy=y0; dy<y1; dy++) { // Bresenham algorithm
		for(int dx=x0; dx<x1; dx++) {
			draw_pixel(dx, dy, color);
		}
	}
}
void draw_text(const int x, const int y, const string& s, const int color) {
	for(int i=0; i<(int)length(s); i++) {
		const int character = (int)s[i];
		const ulong pixels = get_font_pixels(character);
		for(int k=0; k<64; k++) {
			if((pixels>>(63-k))&1) draw_pixel(x+i*6+k%6+(character==113), y+k/6, color);
		}
	}
}
void draw_label(const int x, const int y, const string& s, const int color) {
	draw_text(x, y, s, color);
	if(camera.vr) {
		if(x-camera.width/2>0) {
			draw_text(x-(int)camera.width/2, y, s, color);
		}
		if(x+(int)camera.width/2<(int)camera.width) {
			draw_text(x+(int)camera.width/2, y, s, color);
		}
	}
}
void draw_line_label(const int x0, const int y0, const int x1, const int y1, const int color) {
	draw_line(x0, y0, x1, y1, color);
	if(camera.vr) {
		if((x0+x1)/2-camera.width/2>0) {
			draw_line(x0-(int)camera.width/2, y0, x1-(int)camera.width/2, y1, color);
		}
		if((x0+x1)/2+(int)camera.width/2<(int)camera.width) {
			draw_line(x0+(int)camera.width/2, y0, x1+(int)camera.width/2, y1, color);
		}
	}
}
void draw_bitmap(int* bitmap) {
	std::swap(camera.bitmap, bitmap); // swap pointers instead of memory copy
}

void draw_pixel(const float3& p, const int color) {
	if(!camera.vr) {
		convert_pixel(p, color,  0);
	} else {
		convert_pixel(p, color, -1);
		convert_pixel(p, color, +1);
	}
}
void draw_circle(const float3& p, const float r, const int color) {
	if(!camera.vr) {
		convert_circle(p, r, color,  0);
	} else {
		convert_circle(p, r, color, -1);
		convert_circle(p, r, color, +1);
	}
}
void draw_line(const float3& p0, const float3& p1, const int color) {
	if(!camera.vr) {
		convert_line(p0, p1, color,  0);
	} else {
		convert_line(p0, p1, color, -1);
		convert_line(p0, p1, color, +1);
	}
}
void draw_triangle(const float3& p0, const float3& p1, const float3& p2, const int color, const bool translucent) { // points clockwise from above
	const int cl = shading(color, (p0+p1+p2)/3.0f, cross(p1-p0, p2-p0), translucent);
	if(!camera.vr) {
		convert_triangle(p0, p1, p2, cl,  0);
	} else {
		convert_triangle(p0, p1, p2, cl, -1);
		convert_triangle(p0, p1, p2, cl, +1);
	}
}
void draw_triangle(const float3& p0, const float3& p1, const float3& p2, const int c0, const int c1, const int c2, const bool translucent) { // points clockwise from above
	const float3 normal = cross(p1-p0, p2-p0);
	const int cl0 = shading(c0, p0, normal, translucent);
	const int cl1 = shading(c1, p1, normal, translucent);
	const int cl2 = shading(c2, p2, normal, translucent);
	if(!camera.vr) {
		convert_triangle_interpolated(p0, p1, p2, cl0, cl1, cl2,  0);
	} else {
		convert_triangle_interpolated(p0, p1, p2, cl0, cl1, cl2, -1);
		convert_triangle_interpolated(p0, p1, p2, cl0, cl1, cl2, +1);
	}
}
void draw_text(const float3& p, const float r, const string& s, const int color) {
	if(!camera.vr) {
		convert_text(p, s, r, color,  0);
	} else {
		convert_text(p, s, r, color, -1);
		convert_text(p, s, r, color, +1);
	}
}

void key_bindings(const int key) {
	switch(key) {
		// reserved keys for graphics: W,A,S,D, I,J,K,L, R,U, V,B, C,VK_SPACE, Y,X, N,M
		//case 'A': key_A = !key_A; break;
		//case 'B': key_B = !key_B; break;
		//case 'C': key_C = !key_C; break;
		//case 'D': key_D = !key_D; break;
		case 'E': key_E = !key_E; break;
		//case 'F': key_F = !key_F; break;
		case 'G': key_G = !key_G; break;
		case 'H': key_H = !key_H; break;
		//case 'I': key_I = !key_I; break;
		//case 'J': key_J = !key_J; break;
		//case 'K': key_K = !key_K; break;
		//case 'L': key_L = !key_L; break;
		//case 'M': key_M = !key_M; break;
		//case 'N': key_N = !key_N; break;
		case 'O': key_O = !key_O; break;
		case 'P': key_P = !key_P; break;
		case 'Q': key_Q = !key_Q; break;
		//case 'R': key_R = !key_R; break;
		//case 'S': key_S = !key_S; break;
		case 'T': key_T = !key_T; break;
		//case 'U': key_U = !key_U; break;
		//case 'V': key_V = !key_V; break;
		//case 'W': key_W = !key_W; break;
		//case 'X': key_X = !key_X; break;
		//case 'Y': key_Y = !key_Y; break;
		case 'Z': key_Z = !key_Z; break;
		case '1': key_1 = !key_1; break;
		case '2': key_2 = !key_2; break;
		case '3': key_3 = !key_3; break;
		case '4': key_4 = !key_4; break;
		case '5': key_5 = !key_5; break;
		case '6': key_6 = !key_6; break;
		case '7': key_7 = !key_7; break;
		case '8': key_8 = !key_8; break;
		case '9': key_9 = !key_9; break;
		case '0': key_0 = !key_0; break;
	}
	camera.input_key(key);
}

#if defined(INTERACTIVE_GRAPHICS)
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
HWND window;
HDC displayDC, memDC;
HBITMAP frameDC;
#ifdef BOX_VIEWPORT_6
const uint box_monitor_count = 3u;
HWND box_windows[box_monitor_count] = { nullptr };
HDC box_displayDCs[box_monitor_count] = { nullptr };
HDC box_memDCs[box_monitor_count] = { nullptr };
HBITMAP box_frameDCs[box_monitor_count] = { nullptr };
#endif

int key_windows(const int keycode) {
	switch(keycode) {
		case VK_SPACE     : return ' '; // space
		case VK_ESCAPE    : return  27; // escape
		case VK_UP        : return -38; // up arrow
		case VK_DOWN      : return  40; // down arrow
		case VK_LEFT      : return -37; // left arrow
		case VK_RIGHT     : return -39; // right arrow
		case VK_PRIOR     : return -33; // page up
		case VK_NEXT      : return -34; // page down
		case VK_HOME      : return -36; // pos1
		case VK_ADD       : case VK_OEM_PLUS : return '+'; // +
		case VK_SUBTRACT  : case VK_OEM_MINUS: return '-'; // -
		case VK_MULTIPLY  : return '*'; // *
		case VK_DIVIDE    : return '/'; // /
		case VK_OEM_COMMA : return ','; // ,
		case VK_OEM_PERIOD: return '.'; // .
		default: return keycode;
	}
}
void update_frame(const double frametime) {
	main_label(frametime);
	SetBitmapBits(frameDC, 4*(int)camera.width*(int)camera.height, camera.bitmap);
	BitBlt(displayDC, 0, 0, (int)camera.width, (int)camera.height, memDC, 0, 0, SRCCOPY); // copy back buffer to front buffer
	//camera.clear_frame(); // clear frame
}
LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	if(message==WM_DESTROY) {
		running = false;
		PostQuitMessage(0);
		exit(0);
	} else if(message==WM_MOUSEMOVE) {
		camera.input_mouse_moved((int)LOWORD(lParam), (int)HIWORD(lParam));
		if(!camera.lockmouse) SetCursorPos((int)camera.width/2, (int)camera.height/2); // center cursor
	} else if(message==WM_MOUSEWHEEL) {
		if((short)HIWORD(wParam)>0) camera.input_scroll_up(); else camera.input_scroll_down();
	} else if(message==WM_LBUTTONDOWN||message==WM_MBUTTONDOWN||message==WM_RBUTTONDOWN) {
		if(!camera.lockmouse) {
			ShowCursor(true); // show cursor
		} else {
			ShowCursor(false); // hide cursor
			SetCursorPos(camera.width/2, camera.height/2); // reset cursor
		}
		camera.input_key('U');
	} else if(message==WM_KEYDOWN) {
		int key = key_windows((int)wParam);
		if(key=='U') {
			if(!camera.lockmouse) {
				ShowCursor(true); // show cursor
			} else {
				ShowCursor(false); // hide cursor
				SetCursorPos(camera.width/2, camera.height/2); // reset cursor
			}
		}
		camera.set_key_state(key, true);
		key_bindings(key);
	} else if(message==WM_KEYUP) {
		int key = key_windows((int)wParam);
		camera.set_key_state(key, false);
	}
	return DefWindowProc(window, message, wParam, lParam);
}
#ifdef GRAPHICS_CONSOLE
int main(int argc, char* argv[]) { // call WinMain from dummy main function in order to have an additional console window
	main_arguments = get_main_arguments(argc, argv);
	return WinMain(GetModuleHandle(0), 0, GetCommandLineA(), SW_SHOWMINIMIZED);
}
#endif // GRAPHICS_CONSOLE

#ifdef BOX_VIEWPORT_6
// Layouts, selected in defines.hpp:
//   BOX_VIEWPORT_6_MULTI_MONITOR:  one view per physical monitor
//   BOX_VIEWPORT_6_SINGLE_MONITOR: 6 windows tiled 3x2 on one monitor
//   BOX_VIEWPORT_6_CUBE_NET:       unfolded cube (cross shape) in one window
 
// ----------------------------- shared helpers --------------------------------
// Used by MULTI_MONITOR and SINGLE_MONITOR only (CUBE_NET has its own, see below).
// Zoom for these two layouts: change 40.0f (all views except Bottom) or pass a different
// bottom_zoom (view 0). Higher number = closer / bigger, lower = farther / smaller.
void set_camera_transform_for_view(Camera& cam, int view_index, float bottom_zoom = 85.0f) {
    switch (view_index) {
        case 0: cam.rx = 0.5 * pi; cam.ry = 0.5 * pi; break;
        case 1: cam.rx = 0.0;      cam.ry = pi;       break;
        case 2: cam.rx = 0.5 * pi; cam.ry = pi;       break;
        case 3: cam.rx = 0.5 * pi; cam.ry = 1.5 * pi; break;
        case 4: cam.rx = pi;       cam.ry = pi;       break;
        case 5: cam.rx = 1.5 * pi; cam.ry = pi;       break;
    }
    cam.lockmouse = true;
    cam.set_zoom(view_index == 0 ? bottom_zoom : 40.0f); // <-- zoom for views 1-5 is this 40.0f
    cam.fov = 10.0f;
    cam.update_matrix();
}
 
const string view_labels[6] = {
    "1: Bottom (-Z)",
    "2: Right (+X)",
    "3: Front (+Y)",
    "4: Top (+Z)",
    "5: Left (-X)",
    "6: Back (-Y)"
};
 
struct MonitorLookup {
    int requested_index = 0;
    int current_index = 0;
    RECT rect = { 0, 0, 0, 0 };
    bool found = false;
};
 
BOOL CALLBACK find_monitor(const HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    MonitorLookup* lookup = (MonitorLookup*)data;
    if (lookup->current_index == lookup->requested_index) {
        MONITORINFO info = { sizeof(info) };
        if (GetMonitorInfo(monitor, &info)) {
            lookup->rect = info.rcMonitor;
            lookup->found = true;
        }
        return FALSE;
    }
    lookup->current_index++;
    return TRUE;
}
 
bool get_monitor_rect(const int monitor_index, RECT& rect) {
    MonitorLookup lookup;
    lookup.requested_index = monitor_index;
    EnumDisplayMonitors(nullptr, nullptr, find_monitor, (LPARAM)&lookup);
    if (lookup.found) rect = lookup.rect;
    return lookup.found;
}
 
#if defined(BOX_VIEWPORT_6_MULTI_MONITOR)
// ------------------------------ multi-monitor --------------------------------
bool render_side_to_monitor(const uint view_index, const int monitor_index, const BITMAPINFO& bitmap_info, const double frametime, HDC target_memDC) {
    if (view_index >= 6u || !camera.allow_rendering) return false;
 
    set_camera_transform_for_view(camera, (int)view_index);
    camera.update_state(fmax(1.0 / (double)camera.fps_limit, frametime));
    main_graphics();
    main_label(frametime);
    draw_text(10, 10, view_labels[view_index], 0xFFFFFF);
 
    SetDIBitsToDevice(
        target_memDC,
        0, 0,
        camera.width, camera.height,
        0, 0,
        0, camera.height,
        camera.bitmap,
        &bitmap_info,
        DIB_RGB_COLORS
    );
    return true;
}
 
INT WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PSTR, _In_ INT) {
    WNDCLASS wndClass = { 0, WndProc, 0, 0, hInstance, LoadIcon(0, IDI_APPLICATION), LoadCursor(0, IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), 0, TEXT("WindowClass") };
    RegisterClass(&wndClass);
 
    RECT output_rects[box_monitor_count];
    uint output_widths[box_monitor_count], output_heights[box_monitor_count];
    uint tile_width = 0u, tile_height = 0u;
    const uint monitor_for_display[box_monitor_count] = { 0u, 1u, 2u }; // physical monitor indices
    const uint view_for_display[box_monitor_count] = { 0u, 5u, 4u };    // box views shown on each display
    for (uint i = 0u; i < box_monitor_count; i++) {
        if (!get_monitor_rect((int)monitor_for_display[i], output_rects[i])) return 1;
        output_widths[i] = (uint)(output_rects[i].right - output_rects[i].left);
        output_heights[i] = (uint)(output_rects[i].bottom - output_rects[i].top);
        if (i == 0u) {
            tile_width = output_widths[i];
            tile_height = output_heights[i];
        } else {
            tile_width = min(tile_width, output_widths[i]);
            tile_height = min(tile_height, output_heights[i]);
        }
    }
    DEVMODE lpDevMode = { 0 };
    const uint fps_limit = (uint)EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &lpDevMode) != 0 ? (uint)lpDevMode.dmDisplayFrequency : 60u;
 
    ShowCursor(false);
    for (uint i = 0u; i < box_monitor_count; i++) {
        box_windows[i] = CreateWindowEx(
            WS_EX_TOPMOST,
            "WindowClass",
            WINDOW_NAME,
            WS_POPUP,
            output_rects[i].left,
            output_rects[i].top,
            output_widths[i],
            output_heights[i],
            0,
            0,
            hInstance,
            0
        );
        if (!box_windows[i]) return 1;
        SetWindowPos(box_windows[i], HWND_TOPMOST, output_rects[i].left, output_rects[i].top, output_widths[i], output_heights[i], SWP_SHOWWINDOW);
        ShowWindow(box_windows[i], SW_SHOW);
        UpdateWindow(box_windows[i]);
        box_displayDCs[i] = GetDC(box_windows[i]);
        box_memDCs[i] = CreateCompatibleDC(box_displayDCs[i]);
        box_frameDCs[i] = CreateCompatibleBitmap(box_displayDCs[i], tile_width, tile_height);
        if (!box_displayDCs[i] || !box_memDCs[i] || !box_frameDCs[i]) return 1;
        SelectObject(box_memDCs[i], box_frameDCs[i]);
    }
    window = box_windows[0];
    displayDC = box_displayDCs[0];
    memDC = box_memDCs[0];
    frameDC = box_frameDCs[0];
    SetCursorPos(output_rects[0].left + (int)tile_width / 2, output_rects[0].top + (int)tile_height / 2);
 
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = tile_width;
    bmi.bmiHeader.biHeight = -(int)tile_height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
 
    camera = Camera(tile_width, tile_height, fps_limit);
    thread compute_thread(main_physics);
 
    Clock clock;
    double frametime = 1.0;
    MSG msg = { 0 };
 
    while (msg.message != WM_QUIT) {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;
 
        camera.rendring_frame.lock();
 
        for (uint i = 0u; i < box_monitor_count; i++) {
            render_side_to_monitor(view_for_display[i], (int)monitor_for_display[i], bmi, frametime, box_memDCs[i]);
            SetStretchBltMode(box_displayDCs[i], COLORONCOLOR);
            StretchBlt(box_displayDCs[i], 0, 0, output_widths[i], output_heights[i], box_memDCs[i], 0, 0, tile_width, tile_height, SRCCOPY);
        }
 
        camera.rendring_frame.unlock();
 
        frametime = clock.stop();
        sleep(1.0 / (double)camera.fps_limit - frametime);
        clock.start();
    }
 
    for (uint i = 0u; i < box_monitor_count; i++) {
        SelectObject(box_memDCs[i], NULL);
        DeleteObject(box_frameDCs[i]);
        DeleteDC(box_memDCs[i]);
        ReleaseDC(box_windows[i], box_displayDCs[i]);
    }
 
    if (compute_thread.joinable()) {
        compute_thread.join();
    }
 
    return 0;
}
 
#elif defined(BOX_VIEWPORT_6_SINGLE_MONITOR)
// ----------------------------- single monitor (3x2) --------------------------
INT WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PSTR, _In_ INT) {
    WNDCLASS wndClass = { 0, WndProc, 0, 0, hInstance, LoadIcon(0, IDI_APPLICATION), LoadCursor(0, IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), 0, TEXT("WindowClass") };
    RegisterClass(&wndClass);
 
    const uint monitor_for_display = 1u; // physical monitor index
 
    RECT monitor_rect;
    if (!get_monitor_rect((int)monitor_for_display, monitor_rect)) return 1;
 
    DEVMODE lpDevMode = { 0 };
    const uint width = (uint)(monitor_rect.right - monitor_rect.left);
    const uint height = (uint)(monitor_rect.bottom - monitor_rect.top);
    const uint fps_limit = EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &lpDevMode) != 0 ? (uint)lpDevMode.dmDisplayFrequency : 60u;
 
    // Window 1 | Window 2 | Window 3
    // Window 4 | Window 5 | Window 6
    const uint window_width = width / 3;
    const uint window_height = height / 2;
 
    HWND windows[6] = {};
    HDC displayDCs[6] = {};
    HDC memDCs[6] = {};
    HBITMAP frameDCs[6] = {};
 
    for (int i = 0; i < 6; i++) {
        const int column = i % 3;
        const int row = i / 3;
        const int window_x = monitor_rect.left + column * (int)window_width;
        const int window_y = monitor_rect.top + row * (int)window_height;
 
        windows[i] = CreateWindow(
            "WindowClass",
            view_labels[i].c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            window_x,
            window_y,
            window_width,
            window_height,
            0,
            0,
            hInstance,
            0
        );
        if (!windows[i]) return 1;
 
        displayDCs[i] = GetDC(windows[i]);
        memDCs[i] = CreateCompatibleDC(displayDCs[i]);
        frameDCs[i] = CreateCompatibleBitmap(displayDCs[i], window_width, window_height);
        SelectObject(memDCs[i], frameDCs[i]);
    }
 
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = window_width;
    bmi.bmiHeader.biHeight = -(int)window_height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
 
    camera = Camera(window_width, window_height, fps_limit);
    thread compute_thread(main_physics);
 
    Clock clock;
    double frametime = 1.0;
    MSG msg = { 0 };
 
    while (msg.message != WM_QUIT) {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;
 
        camera.rendring_frame.lock();
 
        if (camera.allow_rendering) {
            for (int i = 0; i < 6; i++) {
                set_camera_transform_for_view(camera, i, 64.0f); // 64.0f = zoom of view 0 (Bottom) in this layout
                camera.update_state(fmax(1.0 / (double)camera.fps_limit, frametime));
                main_graphics();
                draw_text(10, 10, view_labels[i], 0xFFFFFF);
 
                SetDIBitsToDevice(
                    memDCs[i],
                    0, 0,
                    window_width, window_height,
                    0, 0,
                    0, window_height,
                    camera.bitmap,
                    &bmi,
                    DIB_RGB_COLORS
                );
 
                BitBlt(displayDCs[i], 0, 0, window_width, window_height, memDCs[i], 0, 0, SRCCOPY);
            }
        }
 
        camera.rendring_frame.unlock();
 
        frametime = clock.stop();
        sleep(1.0 / (double)camera.fps_limit - frametime);
        clock.start();
    }
 
    for (int i = 0; i < 6; i++) {
        SelectObject(memDCs[i], NULL);
        DeleteObject(frameDCs[i]);
        DeleteDC(memDCs[i]);
        ReleaseDC(windows[i], displayDCs[i]);
        DestroyWindow(windows[i]);
    }
 
    if (compute_thread.joinable()) {
        compute_thread.join();
    }
 
    return 0;
}
 
#elif defined(BOX_VIEWPORT_6_CUBE_NET)
// ------------------------------ cube net --------------------------------------
//
//              [ Top +Z ]                  col 1, row 0
//   [ Left +X ][ Front +Y ][ Right -X ]    cols 0,1,2  row 1
//              [ Bottom -Z ]               col 1, row 2
//              [ Back -Y ]                 col 1, row 3
//
// The cube net remains one connected cross-shaped display.
//
// IMPORTANT:
// The window now uses the entire Windows virtual desktop.
// This means the cube net can extend across multiple physical monitors.
//
// The window is a normal draggable Windows window.
// The mouse cursor remains visible.
//
// Seen from OUTSIDE the cube with Front (+Y) as the anchor, so every shared
// edge is a real cube edge and the net folds into an accurate cube.
//
// All settings are in this block. Index order of every [6] array:
//   0 = Bottom(-Z)
//   1 = +X
//   2 = Front(+Y)
//   3 = Top(+Z)
//   4 = -X
//   5 = Back(-Y)


// ============================================================
// WHICH MONITOR / VIRTUAL DESKTOP
// ============================================================
//
// This is kept for compatibility with the previous code.
//
// -1 = primary monitor
//  0, 1, 2... = monitor by index
//
// The cube-net layout itself now uses the entire virtual desktop,
// so this value is no longer used to limit the cube to one monitor.

const int cube_net_monitor = -1;


// ============================================================
// ZOOM PER FACE
// ============================================================
//
// Higher number = closer / bigger
// Lower number = farther / smaller
//
//                                  Bottom  +X     Front  Top    -X     Back

const float cube_net_zoom[6] = {
    40.0f,
    40.0f,
    40.0f,
    40.0f,
    40.0f,
    40.0f
};


// ============================================================
// FIELD OF VIEW
// ============================================================
//
// Low values make the cube faces look flatter and make the
// shared edges line up better.
//
// 10 degrees is what you were using before.

const float cube_net_fov = 10.0f;


// ============================================================
// CAMERA ANGLES PER FACE
// ============================================================
//
// These put the camera exactly on the labelled face:
//
//   +Y face: rx = pi       ry = pi
//   -Y face: rx = 0        ry = pi
//
//   +X face: rx = .5*pi    ry = pi
//   -X face: rx = 1.5*pi   ry = pi
//
//   +Z face: ry = .5*pi
//   -Z face: ry = 1.5*pi
//
// For +Z / -Z, rx controls which direction is "up".
//
//                                  Bottom     +X       Front   Top      -X        Back

const double cube_net_rx[6] = {
    pi,
    0.5 * pi,
    pi,
    pi,
    1.5 * pi,
    0.0
};

const double cube_net_ry[6] = {
    1.5 * pi,
    pi,
    pi,
    0.5 * pi,
    pi,
    pi
};


// ============================================================
// CROSS-SHAPED CUBE NET POSITION
// ============================================================
//
// This is the part that keeps the original cross.
//
//              [ Top ]
//
// [ Left ][ Front ][ Right ]
//
//              [ Bottom ]
//
//              [ Back ]
//
// 3 columns x 4 rows.
//
//                                  Bottom  +X  Front  Top  -X  Back

const int cube_net_col[6] = {
    1,
    0,
    1,
    1,
    2,
    1
};

const int cube_net_row[6] = {
    2,
    1,
    1,
    0,
    1,
    3
};


// ============================================================
// EXTRA ROTATION PER FACE
// ============================================================
//
// 0 = no rotation
// 1 = 90 degrees clockwise
// 2 = 180 degrees
// 3 = 270 degrees clockwise
//
// Back needs 2 because of the way the camera is oriented.
//
//                                  Bottom  +X  Front  Top  -X  Back

const uint cube_net_rotation[6] = {
    0,
    0,
    0,
    0,
    0,
    2
};


// ============================================================
// BORDER
// ============================================================

const bool cube_net_border = true;

const int cube_net_border_color = 0x808080;


// ============================================================
// CUBE NET SCALE
// ============================================================
//
// 1.0 = as large as possible
// 0.8 = 80%
// 0.5 = 50%
//
// Because the virtual desktop is now used, 1.0 will attempt
// to make the cross as large as possible across all monitors.

const float cube_net_scale = 1.0f;


// ============================================================
// CAMERA SETUP
// ============================================================

void set_camera_transform_cube_net(
    Camera& cam,
    const int view_index
) {
    cam.rx = cube_net_rx[view_index];
    cam.ry = cube_net_ry[view_index];

    // Keep the camera from responding to the mouse.
    // This is important because the mouse is now used for
    // dragging the Windows window.
    cam.lockmouse = true;

    cam.set_zoom(
        cube_net_zoom[view_index]
    );

    cam.fov = cube_net_fov;

    cam.update_matrix();
}


// ============================================================
// OLD MONITOR FUNCTION
// ============================================================
//
// Kept here in case other code still references it.

bool get_cube_net_monitor_rect(RECT& rect) {

    if (cube_net_monitor >= 0)
        return get_monitor_rect(
            cube_net_monitor,
            rect
        );

    const HMONITOR monitor =
        MonitorFromPoint(
            POINT{ 0, 0 },
            MONITOR_DEFAULTTOPRIMARY
        );

    MONITORINFO info = {
        sizeof(info)
    };

    if (!GetMonitorInfo(
        monitor,
        &info
    ))
        return false;

    rect = info.rcMonitor;

    return true;
}


// ============================================================
// ROTATE IMAGE
// ============================================================

void rotate_square_clockwise(
    const uint* src,
    uint* dst,
    const uint n,
    const uint quarter_turns
) {

    for (uint y = 0u; y < n; y++) {

        for (uint x = 0u; x < n; x++) {

            const uint p =
                src[y * n + x];

            switch (quarter_turns & 3u) {

                case 0u:

                    dst[
                        y * n + x
                    ] = p;

                    break;


                case 1u:

                    dst[
                        x * n +
                        (n - 1u - y)
                    ] = p;

                    break;


                case 2u:

                    dst[
                        (n - 1u - y) * n +
                        (n - 1u - x)
                    ] = p;

                    break;


                case 3u:

                    dst[
                        (n - 1u - x) * n +
                        y
                    ] = p;

                    break;
            }
        }
    }
}


void draw_tile_border(
    int* bitmap,
    const uint n,
    const int color
) {

    for (uint i = 0u; i < n; i++) {

        // Top
        bitmap[i] = color;

        // Bottom
        bitmap[
            (n - 1u) * n + i
        ] = color;

        // Left
        bitmap[
            i * n
        ] = color;

        // Right
        bitmap[
            i * n + n - 1u
        ] = color;
    }
}


INT WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ PSTR,
    _In_ INT
) {

    WNDCLASS wndClass = {
        0,
        WndProc,
        0,
        0,
        hInstance,
        LoadIcon(
            0,
            IDI_APPLICATION
        ),
        LoadCursor(
            0,
            IDC_ARROW
        ),
        (HBRUSH)GetStockObject(
            BLACK_BRUSH
        ),
        0,
        TEXT("WindowClass")
    };

    RegisterClass(&wndClass);

    const int virtual_left =
        GetSystemMetrics(
            SM_XVIRTUALSCREEN
        );

    const int virtual_top =
        GetSystemMetrics(
            SM_YVIRTUALSCREEN
        );

    const uint virtual_width =
        (uint)GetSystemMetrics(
            SM_CXVIRTUALSCREEN
        );

    const uint virtual_height =
        (uint)GetSystemMetrics(
            SM_CYVIRTUALSCREEN
        );


    if (
        virtual_width == 0 ||
        virtual_height == 0
    ) {
        return 1;
    }

    DEVMODE lpDevMode = {
        0
    };

    const uint fps_limit =
        EnumDisplaySettings(
            0,
            ENUM_CURRENT_SETTINGS,
            &lpDevMode
        ) != 0
        ?
        (uint)lpDevMode.dmDisplayFrequency
        :
        60u;


    const uint max_tile =
        min(
            virtual_width / 3u,
            virtual_height / 4u
        );

    const uint tile =
        (
            (uint)(
                (float)max_tile *
                cube_net_scale
            )
            / 8u
        ) * 8u;


    if (tile == 0)
        return 1;


    const uint net_width =
        3u * tile;

    const uint net_height =
        4u * tile;

    const int net_offset_x =
        (
            (int)virtual_width -
            (int)net_width
        ) / 2;

    const int net_offset_y =
        (
            (int)virtual_height -
            (int)net_height
        ) / 2;


    ShowCursor(TRUE);
    window = CreateWindowEx(
        0,
        "WindowClass",
        "FluidX3D Cube Net",
        WS_OVERLAPPEDWINDOW,

        virtual_left,
        virtual_top,

        virtual_width,
        virtual_height,

        0,
        0,
        hInstance,
        0
    );


    if (!window)
        return 1;

    ShowWindow(
        window,
        SW_SHOW
    );

    UpdateWindow(
        window
    );

    RECT client_rect;

    GetClientRect(
        window,
        &client_rect
    );


    const uint client_width =
        (uint)(
            client_rect.right -
            client_rect.left
        );

    const uint client_height =
        (uint)(
            client_rect.bottom -
            client_rect.top
        );


    displayDC =
        GetDC(window);

    memDC =
        CreateCompatibleDC(
            displayDC
        );


    frameDC =
        CreateCompatibleBitmap(
            displayDC,
            net_width,
            net_height
        );


    if (
        !displayDC ||
        !memDC ||
        !frameDC
    ) {
        return 1;
    }


    SelectObject(
        memDC,
        frameDC
    );

    PatBlt(
        memDC,
        0,
        0,
        net_width,
        net_height,
        BLACKNESS
    );

    const int draw_offset_x =
        (
            (int)client_width -
            (int)net_width
        ) / 2;

    const int draw_offset_y =
        (
            (int)client_height -
            (int)net_height
        ) / 2;

    BITMAPINFO bmi = {
        0
    };

    bmi.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    bmi.bmiHeader.biWidth =
        tile;

    bmi.bmiHeader.biHeight =
        -(int)tile;

    bmi.bmiHeader.biPlanes =
        1;

    bmi.bmiHeader.biBitCount =
        32;

    bmi.bmiHeader.biCompression =
        BI_RGB;

    camera =
        Camera(
            tile,
            tile,
            fps_limit
        );

    vector<uint> rotated(
        tile * tile
    );

    thread compute_thread(
        main_physics
    );

    Clock clock;

    double frametime = 1.0;

    MSG msg = {
        0
    };

    while (
        msg.message != WM_QUIT
    ) {

        while (
            PeekMessage(
                &msg,
                0,
                0,
                0,
                PM_REMOVE
            )
        ) {

            if (
                msg.message ==
                WM_QUIT
            ) {
                break;
            }

            TranslateMessage(
                &msg
            );

            DispatchMessage(
                &msg
            );
        }


        if (!running)
            break;
        camera.rendring_frame.lock();
        if (
            camera.allow_rendering
        ) {

            for (
                int view = 0;
                view < 6;
                view++
            ) {

                set_camera_transform_cube_net(
                    camera,
                    view
                );

                camera.update_state(
                    fmax(
                        1.0 /
                        (double)camera.fps_limit,
                        frametime
                    )
                );

                main_graphics();
                if (
                    (cube_net_rotation[view] & 3u)
                    != 0u
                ) {

                    rotate_square_clockwise(
                        (const uint*)
                            camera.bitmap,

                        rotated.data(),

                        tile,

                        cube_net_rotation[view]
                    );


                    memcpy(
                        (void*)
                            camera.bitmap,

                        rotated.data(),

                        (size_t)tile *
                        (size_t)tile *
                        sizeof(uint)
                    );
                }


                if (
                    cube_net_border
                ) {

                    draw_tile_border(
                        camera.bitmap,
                        tile,
                        cube_net_border_color
                    );
                }

                draw_text(
                    10,
                    10,
                    view_labels[view],
                    0xFFFFFF
                );

                SetDIBitsToDevice(

                    memDC,

                    (int)(
                        cube_net_col[view] *
                        tile
                    ),

                    (int)(
                        cube_net_row[view] *
                        tile
                    ),

                    tile,
                    tile,

                    0,
                    0,

                    0,
                    tile,

                    camera.bitmap,

                    &bmi,

                    DIB_RGB_COLORS
                );
            }

            BitBlt(
                displayDC,

                draw_offset_x,
                draw_offset_y,

                net_width,
                net_height,

                memDC,

                0,
                0,

                SRCCOPY
            );
        }

        camera.rendring_frame.unlock();

        frametime =
            clock.stop();


        sleep(
            1.0 /
            (double)camera.fps_limit
            -
            frametime
        );


        clock.start();
    }
    SelectObject(
        memDC,
        NULL
    );


    DeleteObject(
        frameDC
    );


    DeleteDC(
        memDC
    );

    ReleaseDC(
        window,
        displayDC
    );
    DestroyWindow(
        window
    );

    if (
        compute_thread.joinable()
    ) {

        compute_thread.join();
    }


    return 0;
}

#endif // BOX_VIEWPORT_6_MULTI_MONITOR / BOX_VIEWPORT_6_SINGLE_MONITOR / BOX_VIEWPORT_6_CUBE_NET
#else

INT WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PSTR, _In_ INT) {
	WNDCLASS wndClass = { 0, WndProc, 0, 0, hInstance, LoadIcon(0, IDI_APPLICATION), LoadCursor(0, IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), 0, TEXT("WindowClass") };
	RegisterClass(&wndClass);
	MONITORINFO mi = { sizeof(mi) };
	if(!GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &mi)) return 1;
	DEVMODE lpDevMode = { 0 }; // get monitor fps
	const uint width  = (uint)(mi.rcMonitor.right-mi.rcMonitor.left); // get screen size, initialize variables
	const uint height = (uint)(mi.rcMonitor.bottom-mi.rcMonitor.top);
	const uint fps_limit = (uint)EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &lpDevMode)!=0 ? (uint)lpDevMode.dmDisplayFrequency : 60u; // find out screen refresh rate
	ShowCursor(false); // hide cursor
	SetCursorPos(width/2, height/2);
	window = CreateWindow("WindowClass", WINDOW_NAME, WS_POPUP|WS_VISIBLE, mi.rcMonitor.left, mi.rcMonitor.top, width, height, 0, 0, hInstance, 0); // create fullscreen window
	displayDC = GetDC(window);
	memDC = CreateCompatibleDC(displayDC);
	frameDC = CreateCompatibleBitmap(displayDC, width, height); // initialize back buffer
	SelectObject(memDC, frameDC);

	camera = Camera(width, height, fps_limit);
	thread compute_thread(main_physics); // start main_physics() in a new thread

	Clock clock;
	double frametime = 1.0;
	MSG msg = { 0 };
	while(msg.message!=WM_QUIT) {
		while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if(msg.message==WM_QUIT) break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if(!running) break;
		// main loop ################################################################
		camera.rendring_frame.lock(); // block rendering for other threads until finished
		camera.update_state(fmax(1.0/(double)camera.fps_limit, frametime));
		main_graphics();
		update_frame(frametime);
		camera.rendring_frame.unlock();
		frametime = clock.stop();
		sleep(1.0/(double)camera.fps_limit-frametime);
		clock.start();
		// ##########################################################################
	}

	ReleaseDC(window, displayDC);
	compute_thread.join();
	return 0;
}



#endif


#elif defined(__linux__)||defined(__APPLE__)

#include "X11/include/X11/Xlib.h" // sources: libx11-dev, x11proto-dev, libxrandr-dev, libxrender-dev
#define register // to avoid compiler warning in XKBlib.h
#include "X11/include/X11/XKBlib.h" // for keyboard layout
#undef register // to avoid compiler warning in XKBlib.h
#include "X11/include/X11/extensions/Xrandr.h" // for multi-monitor

Display* x11_display;
Window x11_window;
GC x11_gc;
XImage* x11_image;
std::atomic_bool x11_cursor_movement_captured = true;

int key_linux(const int keycode) {
	const int keysym = (int)XkbKeycodeToKeysym(x11_display, keycode, 0, 0);
	switch(keysym) { // keysym to upper case ASCII and Windows Virtual-Key Codes (negative numbers)
		case 65293: return   10; // enter
		case 65288: return    8; // backspace
		case 65307: return   27; // escape
		case 65535: return  127; // delete
		case   228: return  142; // Ä
		case   246: return  153; // Ö
		case   252: return  154; // Ü
		case   223: return  225; // ß
		case 65106: return  248; // ^
		case 65105: return  239; // ´
		case 65379: return  -45; // insert
		case 65407: return -144; // num lock
		case 65509: return  -20; // caps lock
		case 65515: return  -91; // windows key
		case 65383: return  -93; // kontext menu key
		case 65436: return  '1'; case 65432: return  '6'; // numpad
		case 65433: return  '2'; case 65429: return  '7'; // numpad
		case 65435: return  '3'; case 65431: return  '8'; // numpad
		case 65430: return  '4'; case 65434: return  '9'; // numpad
		case 65437: return  '5'; case 65438: return  '0'; // numpad
		case 65451: return  '+'; case 65453: return  '-'; // numpad
		case 65450: return  '*'; case 65455: return  '/'; // numpad
		case 65439: return  ','; case 65421: return   10; // numpad
		case 65362: return  -38; case 65364: return  -40; // up/down arrow
		case 65361: return  -37; case 65363: return  -39; // left/right arrow
		case 65365: return  -33; case 65366: return  -34; // page up/down
		case 65360: return  -36; case 65367: return  -35; // pos1/end
		case 65470: return -112; case 65476: return -118; // F1/F7
		case 65471: return -113; case 65477: return -119; // F2/F8
		case 65472: return -114; case 65478: return -120; // F3/F9
		case 65473: return -115; case 65479: return -121; // F4/F10
		case 65474: return -116; case 65480: return -122; // F5/F11
		case 65475: return -117; case 65481: return -123; // F6/F12
		default: return keysym>96&&keysym<123 ? keysym-32 : keysym; // convert letters to upper case
	}
}
void update_frame(const double frametime) {
	main_label(frametime);
	x11_image->data = (char*)camera.bitmap; // camera.bitmap pointer might have been changed, so update x11_image->data pointer here
	XLockDisplay(x11_display);
	XPutImage(x11_display, x11_window, x11_gc, x11_image, 0, 0, 0, 0, camera.width, camera.height);
	if(x11_cursor_movement_captured&&!camera.lockmouse) { // to avoid cursor being centered before its movement has been captured as input
		XWarpPointer(x11_display, x11_window, x11_window, 0, 0, camera.width, camera.height, camera.width/2, camera.height/2); // center cursor
		x11_cursor_movement_captured = false;
	}
	XUnlockDisplay(x11_display);
	//camera.clear_frame(); // clear frame
}
void hide_cursor() {
	const char zeroes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	Pixmap x11_pixmap = XCreateBitmapFromData(x11_display, x11_window, zeroes, 8, 8);
	XColor black;
	black.red = black.green = black.blue = 0;
	Cursor x11_invisible_cursor = XCreatePixmapCursor(x11_display, x11_pixmap, x11_pixmap, &black, &black, 0, 0);
	XDefineCursor(x11_display, x11_window, x11_invisible_cursor);
	XFreeCursor(x11_display, x11_invisible_cursor);
	XFreePixmap(x11_display, x11_pixmap);
}
void show_cursor() {
	Cursor x11_cursor = XCreateFontCursor(x11_display, 68);
	XDefineCursor(x11_display, x11_window, x11_cursor);
	XFreeCursor(x11_display, x11_cursor);
}
void input_detection() {
	XEvent x11_event;
	while(running) {
		XNextEvent(x11_display, &x11_event);
		if(!x11_cursor_movement_captured&&x11_event.type==MotionNotify) { // to avoid cursor movement being captured before cursor has been centered
			camera.input_mouse_moved((int)x11_event.xmotion.x, (int)x11_event.xmotion.y);
			x11_cursor_movement_captured = true;
		} else if(x11_event.type==ButtonPress) { // counterpart: ButtonRelease
			const int x11_button = x11_event.xbutton.button;
			if(x11_button==Button4) { // scroll up
				camera.input_scroll_up();
			} else if(x11_button==Button5) { // scroll down
				camera.input_scroll_down();
			} else if(x11_button==Button1||x11_button==Button2||x11_button==Button3) {
				if(!camera.lockmouse) {
					show_cursor();
				} else {
					hide_cursor();
					XWarpPointer(x11_display, x11_window, x11_window, 0, 0, camera.width, camera.height, (int)camera.width/2, (int)camera.height/2); // reset cursor
				}
				camera.input_key('U');
			}
		} else if(x11_event.type==KeyPress) {
			const int key = key_linux((int)x11_event.xkey.keycode);
			if(key=='U') {
				if(!camera.lockmouse) {
					show_cursor();
				} else {
					hide_cursor();
					XWarpPointer(x11_display, x11_window, x11_window, 0, 0, camera.width, camera.height, (int)camera.width/2, (int)camera.height/2); // reset cursor
				}
			}
			camera.set_key_state(key, true);
			key_bindings(key);
		} else if(x11_event.type==KeyRelease) {
			const int key = key_linux((int)x11_event.xkey.keycode);
			camera.set_key_state(key, false);
		}
	}
}
int main(int argc, char* argv[]) {
	main_arguments = get_main_arguments(argc, argv);

	XInitThreads();
	x11_display = XOpenDisplay(0);
	if(!x11_display) print_error("No X11 display available.");

	uint width=0u, height=0u, fps_limit=0u;
	int window_offset_x=0, window_offset_y=0;

	Window x11_root_window = DefaultRootWindow(x11_display);
	XRRScreenResources* x11_screen_resources = XRRGetScreenResources(x11_display, x11_root_window);
	if(x11_screen_resources!=nullptr) {
		RROutput x11_output_primary = XRRGetOutputPrimary(x11_display, x11_root_window);
		if(x11_output_primary!=None) {
			XRROutputInfo* x11_output_info = XRRGetOutputInfo(x11_display, x11_screen_resources, x11_output_primary);
			if(x11_output_info!=nullptr) {
				XRRCrtcInfo* x11_crtc_info = XRRGetCrtcInfo(x11_display, x11_screen_resources, x11_output_info->crtc);
				if(x11_crtc_info!=nullptr) {
					width  = (uint)x11_crtc_info->width; // width and height of primary monitor
					height = (uint)x11_crtc_info->height;
					window_offset_x = (int)x11_crtc_info->x; // offset of primary monitor in multi-monitor coordinates
					window_offset_y = (int)x11_crtc_info->y;
				}
				XRRFreeCrtcInfo(x11_crtc_info);
			}
			XRRFreeOutputInfo(x11_output_info);
		}
	}
	XRRFreeScreenResources(x11_screen_resources);
	XRRScreenConfiguration* x11_screen_configuration = XRRGetScreenInfo(x11_display, x11_root_window);
	if(x11_screen_configuration!=nullptr) {
		fps_limit = (uint)XRRConfigCurrentRate(x11_screen_configuration);
	}
	XRRFreeScreenConfigInfo(x11_screen_configuration);
	if(width==0u||height==0u) { // fallback if Xrandr fails
		width  = (uint)DisplayWidth (x11_display, 0);
		height = (uint)DisplayHeight(x11_display, 0);
		window_offset_x = 0;
		window_offset_y = 0;
	}
	if(fps_limit==0u) {
		fps_limit = 60u; // fallback to 60fps default
	}

	camera = Camera(width, height, fps_limit);

	x11_window = XCreateWindow(x11_display, x11_root_window, window_offset_x, window_offset_y, width, height, 0, CopyFromParent, CopyFromParent, CopyFromParent, 0, 0);
	XSizeHints x11_size_hints = { PPosition|PSize, window_offset_x, window_offset_y, (int)width, (int)height };
	XSetNormalHints(x11_display, x11_window, &x11_size_hints); // place window on the primary monitor
	struct Hints { long flags=2l, functions=0l, decorations=0b0000000l, input_mode=0l, status=0l; } x11_hints; // decorations=maximize|minimize|menu|title|resize|border|all
	Atom x11_property = XInternAtom(x11_display, "_MOTIF_WM_HINTS", 0);
	XChangeProperty(x11_display, x11_window, x11_property, x11_property, 32, PropModeReplace, (unsigned char*)&x11_hints, 5); // remove window decorations
	XStoreName(x11_display, x11_window, WINDOW_NAME);
	XMapRaised(x11_display, x11_window);
	x11_gc = XCreateGC(x11_display, x11_window, 0, 0);
	x11_image = XCreateImage(x11_display, CopyFromParent, DefaultDepth(x11_display, DefaultScreen(x11_display)), ZPixmap, 0, (char*)camera.bitmap, width, height, 32, 0);
	XSelectInput(x11_display, x11_window, KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask);
	hide_cursor();
	XWarpPointer(x11_display, None, x11_window, 0, 0, camera.width, camera.height, window_offset_x+(int)camera.width/2, window_offset_y+(int)camera.height/2); // catch cursor from anywhere on monitors

	thread compute_thread(main_physics); // start main_physics() in a new thread
	thread input_thread(input_detection);

	Clock clock;
	double frametime = 1.0;
	while(running) {
		// main loop ################################################################
		camera.rendring_frame.lock(); // block rendering for other threads until finished
		camera.update_state(fmax(1.0/(double)camera.fps_limit, frametime));
		main_graphics();
		update_frame(frametime);
		camera.rendring_frame.unlock();
		frametime = clock.stop();
		sleep(1.0/(double)camera.fps_limit-frametime);
		clock.start();
		// ##########################################################################
	}

	XFreeGC(x11_display, x11_gc);
	x11_image->data = nullptr; // XDestroyImage would double-delete x11_image->data pointer, so remove pointer here
	XDestroyImage(x11_image);
	XDestroyWindow(x11_display, x11_window);
	XCloseDisplay(x11_display);
	compute_thread.join();
	input_thread.join();
	return 0;
}

#endif // Linux
#endif // INTERACTIVE_GRAPHICS

#ifdef INTERACTIVE_GRAPHICS_ASCII

uint fontwidth=8u, fontheight=16u;
void update_frame(const double frametime) {
	uint textwidth=0u, textheight=0u;
	get_console_size(textwidth, textheight);
	textwidth = max(textwidth, 2u)-1u;
	textwidth = min(textwidth, textheight*camera.width*fontheight/(camera.height*fontwidth));
	textheight = min(textheight, textwidth*camera.height*fontwidth/(camera.width*fontheight));
	textwidth = max(textwidth, 1u);
	textheight = max(textheight, 1u);
	show_console_cursor(false);
	const Image image(camera.width, camera.height, camera.bitmap);
	print_video(&image, textwidth, textheight);
	print(alignr(textwidth, to_string(textwidth)+"x"+to_string(textheight)+" "+alignr(4, to_int(1.0/frametime))+"fps"));
	show_console_cursor(true);
	//camera.clear_frame(); // clear frame
}
void input_detection() {
	while(running) {
		int key = key_press();
		key -= (key>96&&key<123)*32; // convert lower case to upper case
		key_bindings(key);
	}
}
int main(int argc, char* argv[]) {
	main_arguments = get_main_arguments(argc, argv);
	camera = Camera(384u, 216u, 60u); // width and height must be divisible by 8
	thread compute_thread(main_physics); // start main_physics() in a new thread
	thread input_thread(input_detection);
	Clock clock;
	double frametime = 1.0;
	get_console_font_size(fontwidth, fontheight);
	while(running) {
		// main loop ################################################################
		camera.rendring_frame.lock(); // block rendering for other threads until finished
		camera.update_state(fmax(1.0/(double)camera.fps_limit, frametime));
		main_graphics();
		if(camera.allow_rendering) update_frame(frametime);
		camera.rendring_frame.unlock();
		frametime = clock.stop();
		sleep(1.0/(double)camera.fps_limit-frametime);
		clock.start();
		// ##########################################################################
	}
	compute_thread.join();
	input_thread.join();
	return 0;
}

#endif // INTERACTIVE_GRAPHICS_ASCII

#if !defined(INTERACTIVE_GRAPHICS) && !defined(INTERACTIVE_GRAPHICS_ASCII)

int main(int argc, char* argv[]) {
	main_arguments = get_main_arguments(argc, argv);
	camera = Camera(GRAPHICS_FRAME_WIDTH, GRAPHICS_FRAME_HEIGHT, 60u); // width and height must be divisible by 8
	thread compute_thread(main_physics); // start main_physics() in a new thread
	while(running) {
		// main loop ################################################################
		main_label(1.0);
		sleep(0.050);
		// ##########################################################################
	}
	compute_thread.join();
	return 0;
}

#endif // no INTERACTIVE_GRAPHICS and no INTERACTIVE_GRAPHICS_ASCII
#endif // GRAPHICS