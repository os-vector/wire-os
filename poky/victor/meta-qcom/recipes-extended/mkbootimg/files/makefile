# This relies on ${LIBS} for an appropriate path to libmincrypt.a. The path in
# the Android tree is
# out/host/linux-x86/obj/STATIC_LIBRARIES/libmincrypt_intermediates/libmincrypt.a
mkbootimg: mkbootimg.o
	$(CXX) $(LDFLAGS) $< ${LIBS} -o $@

mkbootimg.o: mkbootimg.c bootimg.h
	$(CC) $(INCLUDES) $(CFLAGS) -c $<

clean:
	rm mkbootimg.o