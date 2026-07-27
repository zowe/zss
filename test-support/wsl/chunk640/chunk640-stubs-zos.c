/* chunk640-stubs-zos.c -- no-op stubs for symbols the chunk parser never
 * reaches (crypto/ICSF, GSK/TLS, JWT, ZIS client, configmgr). Generated from the
 * z/OS binder's IEW2456E UNRESOLVED list; present only to satisfy the link for
 * the focused #640 harness. If any of these were on the parser path the --self
 * body echo would be wrong, so a PASS also confirms they are genuinely unreached.
 */
long CSNEOWH(void){return 0;}
long CSNERNGL(void){return 0;}
long CSNESYD(void){return 0;}
long CSNESYE(void){return 0;}
long RSIDCALN(void){return 0;}
long RSIDCCCM(void){return 0;}
long RSIDCDCM(void){return 0;}
long RSIDCDCO(void){return 0;}
long RSIDCEXE(void){return 0;}
long RSIDCPCO(void){return 0;}
long cfgAddConfig(void){return 0;}
long cfgGetConfigData(void){return 0;}
long cfgLoadConfiguration(void){return 0;}
long cfgLoadSchemas(void){return 0;}
long cfgSetConfigPath(void){return 0;}
long cfgSetParmlibMemberName(void){return 0;}
long cfgSetTraceLevel(void){return 0;}
long cfgValidate(void){return 0;}
long cs(void){return 0;}
long gsk_attribute_get_cert_info(void){return 0;}
long gsk_attribute_set_buffer(void){return 0;}
long gsk_attribute_set_callback(void){return 0;}
long gsk_attribute_set_enum(void){return 0;}
long gsk_attribute_set_numeric_value(void){return 0;}
long gsk_environment_close(void){return 0;}
long gsk_environment_init(void){return 0;}
long gsk_environment_open(void){return 0;}
long gsk_free_cert_data(void){return 0;}
long gsk_secure_socket_close(void){return 0;}
long gsk_secure_socket_init(void){return 0;}
long gsk_secure_socket_open(void){return 0;}
long gsk_secure_socket_read(void){return 0;}
long gsk_secure_socket_write(void){return 0;}
long gsk_strerror(void){return 0;}
long jwtAreBasicClaimsValid(void){return 0;}
long jwtVerifyAndParseToken(void){return 0;}
long makeConfigManager(void){return 0;}
long makeJwtContextCustom(void){return 0;}
long makeJwtContextForKeyInToken(void){return 0;}
long zisCallNWMService(void){return 0;}
long zisCheckUsername(void){return 0;}
long zisCheckUsernameAndPassword(void){return 0;}
long zisCopyDataFromAddressSpace(void){return 0;}
long zisGetDefaultServerName(void){return 0;}
