CFLAGS=-g -W `pkg-config --cflags gtk+-2.0`
LDFLAGS=`pkg-config --libs gtk+-2.0`

pager: main.c xutils.c
	$(LINK.c) -o $@ $^

clean:
	rm -f pager
