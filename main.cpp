#include "uarttotcp.h"

int main()
{
	UARTTOTCP uarttotcp("/dev/ttyACM0",57600,8888);
	uarttotcp.setConnectedJudge(true);
	uarttotcp.start();
	return 0;
}
