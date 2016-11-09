CFLAGS += -std=c++11 -O2
CFLAGS += `pkg-config --cflags opencv`
CC = g++ ${CFLAGS}

SPINNAKER_LIB = -L/opt/spinnaker/lib -lSpinnaker${D}
OPENCV_LIB = `pkg-config --libs opencv`

INC = -I/opt/spinnaker/include
LIB += -Wl,-Bdynamic ${SPINNAKER_LIB} ${OPENCV_LIB}
LIB += -lpthread

single_demo: single_demo.o
	${CC} -o single_demo single_demo.o ${LIB}

trigger_demo: trigger_demo.o
	${CC} -o trigger_demo trigger_demo.o ${LIB}

binocular_demo: binocular_demo.o
	${CC} -o binocular_demo binocular_demo.o ${LIB}

%.o: %.cpp
	${CC} ${CFLAGS} ${INC} -Wall -c -D LINUX $*.cpp

clean_obj:
	rm -f *.o	@echo "all cleaned up!"

clean:
	rm -f single_demo trigger_demo binocular_demo *.o	@echo "all cleaned up!"
