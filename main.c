#include <iostream>
#include <Windows.h>

template <typename T> struct vec2 { T x, y; };
typedef vec2<int> vec2i;
typedef vec2<float> vec2f;

#ifndef PI
#define PI 3.14
#endif

#define clamp(val, minval, maxval) min(maxval, max(val, minval))

float deg2rad(float x) { return x * PI / 180; }
float rad2deg(float x) { return x * 180 / PI; }

template <typename T> T sqr(T x) { return x*x; }
float frac(float x) { return x - (int)x; }

#define MAX_BLOCKS_NUM 10
#define MAX_ROUNDED_DIRECTION 10

struct block
{
	vec2i position;
	bool isDeath;
};

struct state
{
	vec2f ballPosition;
	vec2f ballVelocity;
	vec2i playerPosition;
	vec2i windowSize;
	int playerWidth;
	block blocks[MAX_BLOCKS_NUM];
};

void clearConsole()
{
	COORD topLeft = { 0, 0 };
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO screen;
	DWORD written;
	GetConsoleScreenBufferInfo(console, &screen);
	FillConsoleOutputCharacterA(console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
	FillConsoleOutputAttribute(console, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE, screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
	SetConsoleCursorPosition(console, topLeft);
}

void printState(const state& s)
{
	clearConsole();
	size_t size = s.windowSize.x * s.windowSize.y;
	char* mapStr = (char*)malloc(size);
	memset(mapStr, ' ', size);
	for (int i = 0; i < MAX_BLOCKS_NUM; i++)
	{
		if (s.blocks[i].isDeath) { continue; }
		mapStr[s.blocks[i].position.x + s.blocks[i].position.y * s.windowSize.x] = 'x';
	}
	mapStr[(int)(s.ballPosition.x) + (int)(s.ballPosition.y) * s.windowSize.x] = 'o';
	memset(&mapStr[s.playerPosition.x + s.playerPosition.y * s.windowSize.x], '-', s.playerWidth);
	char* tmp = (char*)malloc(s.windowSize.x + 1);
	tmp[s.windowSize.x] = 0;
	for (int i = s.windowSize.y - 1; i >= 0; i--)
	{
		memcpy(tmp, mapStr + i * s.windowSize.x, s.windowSize.x);
		printf("|%s|\n", tmp);
	}
	free(tmp);
	free(mapStr);
}

int getQSize(const state& s)
{
	return
		s.windowSize.x * // ball position
		s.windowSize.y *
		MAX_ROUNDED_DIRECTION * // ball rotation (rounded)
		(1 << MAX_BLOCKS_NUM) * // alive blocks
		(s.windowSize.x - s.playerWidth + 1) * // player position
		2; // actions
}

int getQIndex(const state& s, int a)
{
	int depths[] = { 1, s.windowSize.x, s.windowSize.y, MAX_ROUNDED_DIRECTION ,(1 << MAX_BLOCKS_NUM), (s.windowSize.x - s.playerWidth + 1) };
	int blockFlags = 0;
	for (int i = 0; i < MAX_BLOCKS_NUM; i++) { if (!s.blocks[i].isDeath) { blockFlags |= 1 << i; } }
	int ballRoundedRotationIndex = 0;
	float rot = atan2(s.ballVelocity.y, s.ballVelocity.x);
	if (rot < 0) { rot += 2 * PI; }
	for (int i = 0; i < MAX_ROUNDED_DIRECTION; i++)
		if (rot <= (i + 1) * (2 * PI / MAX_ROUNDED_DIRECTION))
		{
			ballRoundedRotationIndex = i;
			break;
		}
	int data[] = { s.ballPosition.x, s.ballPosition.y, ballRoundedRotationIndex, blockFlags, s.playerPosition.x, a };
	int sum = 0;
	int depth = 1;
	for (int i = 0; i < sizeof(data) / sizeof(*data); i++)
	{
		depth *= depths[i];
		sum += data[i] * depth;
	}
	return sum;
}

int* createQTable(const state& s) { return new int[getQSize(s)](); }
int getQValue(int *t, const state& s, int a) { return t[getQIndex(s, a)]; }
void setQValue(int *t, const state& s, int a, int q) { t[getQIndex(s, a)] = q; }

void initState(state& s);

#define MOVE_LEFT 0
#define MOVE_RIGHT 1

#define R_LOSE -50
#define R_WIN 100

state doAction(state s, int a, int& r)
{
	if (!a) { s.playerPosition.x--; }
	else { s.playerPosition.x++; }
	s.playerPosition.x = clamp(s.playerPosition.x, 0, s.windowSize.x - s.playerWidth);

	bool win = true;
	for (int i = 0; i < MAX_BLOCKS_NUM; i++)
	{
		if (!s.blocks[i].isDeath)
		{
			win = false;
			if ((sqr(s.blocks[i].position.x - s.ballPosition.x) +
				sqr(s.blocks[i].position.y - s.ballPosition.y)) <= 2)
			{
				float tx, ty;
				for (float t = .001; t <= 1; t += 0.2)
					if (int(tx = s.ballPosition.x + s.ballVelocity.x * t) == s.blocks[i].position.x &&
						int(ty = s.ballPosition.y + s.ballVelocity.y * t) == s.blocks[i].position.y)
					{
						s.blocks[i].isDeath = true;
						if (sqr(frac(tx) - .5) >= sqr(frac(ty) - .5)) { s.ballVelocity.x *= -1; }
						else { s.ballVelocity.y *= -1; }
						r = 50;
						return s;
					}
			}
		}
	}
		if (win) { r = R_WIN; return s; }

		vec2f target = { s.ballPosition.x + s.ballVelocity.x, s.ballPosition.y + s.ballVelocity.y };

		bool done = false;

		if (target.x <= 0 || target.x >= s.windowSize.x) { s.ballVelocity.x *= -1; done = true; }
		if (target.y >= s.windowSize.y) { s.ballVelocity.y *= -1; done = true; }
		if (done) { r = -1; return s; }

		int l;

		if ((int)target.y == s.playerPosition.y)
			if ((l = ((int)target.x - s.playerPosition.x)) <= s.playerWidth && l >= -1)
			{
				s.ballVelocity.y *= -1;
				s.ballVelocity.x += (s.ballPosition.x - ((float)s.playerPosition.x + (float)s.playerWidth / 2)) * .5f;
				float len = sqrt(sqr(s.ballVelocity.x) + sqr(s.ballVelocity.y));
				s.ballVelocity.x /= len;
				s.ballVelocity.y /= len;
				r = -1;
				return s;
			}
			else { r = R_LOSE; return s; }

			s.ballPosition = target;
			r = -1;
			return s;
}

void initState(state& s)
{
	s.ballPosition = { 6, 10 };
	float initAngle = deg2rad(-30);
	s.ballVelocity = { cos(initAngle), sin(initAngle) };
	s.playerPosition = { 4, 2 };
	s.playerWidth = 3;
	s.windowSize = { 15,20 };
	for (int i = 0; i < MAX_BLOCKS_NUM; i++)
		s.blocks[i] = { i % s.windowSize.x, s.windowSize.y - 2 - i / s.windowSize.x - i / 4 * 2, false };
}

void play(int *t, state s)
{
	int r;
	while (true)
	{
		int q1, q2;
		if ((q1 = getQValue(t, s, 0)) == (q2 = getQValue(t, s, 1)))
			s = doAction(s, rand() & 1, r);
		else if(q1 > q2)
			s = doAction(s, 0, r);
		else
			s = doAction(s, 1, r);
		printState(s);
		if ((r == R_LOSE) || (r == R_WIN)) { return; }
		Sleep(20);
	}
}

void train(int *t, state s, int episode, float gamma)
{
	state s0 = s;
	int r, a = rand() & 1;
	for(int i = 0; i < episode; i++)
		while (true)
		{
			state s2 = doAction(s, a, r);
			if ((r == R_LOSE) || (r == R_WIN))
			{
				setQValue(t, s, a, r);
				s = s0;
				a = rand() & 1;
				break;
			}
			int q1, q2;
			q1 = getQValue(t, s2, 0);
			q2 = getQValue(t, s2, 1);
			setQValue(t, s, a, (float)r + gamma * (float)max(q1, q2));
			if ((q1 == q2) || !(rand() % 5)) { a = rand() & 1; }
			else if (q1 > q2) { a = 0; }
			else { a = 1; }
			s = s2;
		}
}

int main()
{
	state s;
	initState(s);
	int *t = createQTable(s);
	while (true)
	{
		train(t, s, 250000, .8);
		play(t, s);
	}
	free(t);
	return 0;
}
