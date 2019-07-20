WINDOW = lua-window.a

IMGUI = imgui.o imgui_draw.o imgui_widgets.o lua-render.o lua-imgui.o

all: imgui.dll

imgui.dll: $(IMGUI) $(WINDOW)
	g++ --shared -o $@ $^ -llua -lopengl32 -lglew32 -luser32 -lgdi32

lua-window.a: lua-window.c
	gcc -Wall -c -o lua-window.o $^
	ar -r lua-window.a lua-window.o

$(IMGUI): %.o : %.cpp
	g++ -Wall -c -o $@ $<

clean:
	rm *.a
	rm *.o
	rm *.dll