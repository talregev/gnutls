void WRITEdatum16( opaque* dest, gnutls_datum dat);
void WRITEdatum24( opaque* dest, gnutls_datum dat);
void WRITEdatum32( opaque* dest, gnutls_datum dat);
void WRITEdatum8( opaque* dest, gnutls_datum dat);

int gnutls_set_datum( gnutls_datum* dat, const void* data, int data_size);
void gnutls_free_datum( gnutls_datum* dat);
