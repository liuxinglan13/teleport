#include "stdafx.h"

#pragma warning(disable:4091)

#include <commdlg.h>
#include <ShlObj.h>

#include <teleport_const.h>

#include "ts_http_rpc.h"
#include "dlg_main.h"
#include "ts_ver.h"

/*
1.
SecureCRT֧�����ñ�ǩҳ�ı��⣬�����в��� /N "tab name"�Ϳ���
Example:
To launch a new Telnet session, displaying the name "Houston, TX" on the tab, use the following:
/T /N "Houston, TX" /TELNET 192.168.0.6

2.
���������SecureCRT�ŵ�һ�����ڵĲ�ͬ��ǩҳ�У�ʹ�ò�����  /T
  SecureCRT.exe /T /N "TP#ssh://192.168.1.3" /SSH2 /L root /PASSWORD 1234 120.26.109.25

3.
telnet�ͻ��˵�������
  putty.exe telnet://administrator@127.0.0.1:52389
�����SecureCRT������Ҫ
  SecureCRT.exe /T /N "TP#telnet://192.168.1.3" /SCRIPT X:\path\to\startup.vbs /TELNET 127.0.0.1 52389
���У�startup.vbs������Ϊ��
---------�ļ���ʼ---------
#$language = "VBScript"
#$interface = "1.0"
Sub main
  crt.Screen.Synchronous = True
  crt.Screen.WaitForString "ogin: "
  crt.Screen.Send "SESSION-ID" & VbCr
  crt.Screen.Synchronous = False
End Sub
---------�ļ�����---------

4. Ϊ����putty�Ĵ��ڱ�ǩ��ʾ������IP�����Գ��������ӳɹ������������˷����������
	PS1="\[\e]0;${debian_chroot:+($debian_chroot)}\u@192.168.1.2: \w\a\]$PS1"
�ֹ������ˣ�ubuntu���������ԣ���֪���Ƿ��ܹ�֧�����е�Linux��SecureCRT�Դ˱�ʾ���ԡ�
*/

//#define RDP_CLIENT_SYSTEM_BUILTIN
// #define RDP_CLIENT_SYSTEM_ACTIVE_CONTROL
//#define RDP_CLIENT_FREERDP


//#ifdef RDP_CLIENT_SYSTEM_BUILTIN

std::string rdp_content = "\
connect to console:i:%d\n\
screen mode id:i:%d\n\
use multimon:i:0\n\
desktopwidth:i:%d\n\
desktopheight:i:%d\n\
session bpp:i:16\n\
winposstr:s:0,1,%d,%d,%d,%d\n\
compression:i:1\n\
keyboardhook:i:2\n\
audiocapturemode:i:0\n\
videoplaybackmode:i:1\n\
connection type:i:7\n\
networkautodetect:i:1\n\
bandwidthautodetect:i:1\n\
displayconnectionbar:i:1\n\
enableworkspacereconnect:i:0\n\
disable wallpaper:i:1\n\
allow font smoothing:i:0\n\
allow desktop composition:i:0\n\
disable full window drag:i:1\n\
disable menu anims:i:1\n\
disable themes:i:1\n\
disable cursor setting:i:0\n\
bitmapcachepersistenable:i:1\n\
full address:s:%s:%d\n\
audiomode:i:0\n\
redirectprinters:i:0\n\
redirectcomports:i:0\n\
redirectsmartcards:i:0\n\
redirectclipboard:i:1\n\
redirectposdevices:i:0\n\
autoreconnection enabled:i:0\n\
authentication level:i:2\n\
prompt for credentials:i:0\n\
negotiate security layer:i:1\n\
remoteapplicationmode:i:0\n\
alternate shell:s:\n\
shell working directory:s:\n\
gatewayhostname:s:\n\
gatewayusagemethod:i:4\n\
gatewaycredentialssource:i:4\n\
gatewayprofileusagemethod:i:0\n\
promptcredentialonce:i:0\n\
gatewaybrokeringtype:i:0\n\
use redirection server name:i:0\n\
rdgiskdcproxy:i:0\n\
kdcproxyname:s:\n\
drivestoredirect:s:*\n\
username:s:%s\n\
";

//redirectdirectx:i:0\n\
//prompt for credentials on client:i:0\n\

//#endif


TsHttpRpc g_http_interface;

void http_rpc_main_loop(void)
{
	if (!g_http_interface.init(TS_HTTP_RPC_HOST, TS_HTTP_RPC_PORT))
	{
		EXLOGE("[ERROR] can not start HTTP-RPC listener, maybe port %d is already in use.\n", TS_HTTP_RPC_PORT);
		return;
	}

	EXLOGW("======================================================\n");
	EXLOGW("[rpc] TeleportAssist-HTTP-RPC ready on %s:%d\n", TS_HTTP_RPC_HOST, TS_HTTP_RPC_PORT);

	g_http_interface.run();

	EXLOGW("[rpc] main loop end.\n");
}

void http_rpc_stop(void)
{
	g_http_interface.stop();
}

#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

int ts_url_decode(const char *src, int src_len, char *dst, int dst_len, int is_form_url_encoded)
{
	int i, j, a, b;

	for (i = j = 0; i < src_len && j < dst_len - 1; i++, j++)
	{
		if (src[i] == '%')
		{
			if (i < src_len - 2 && isxdigit(*(const unsigned char *)(src + i + 1)) &&
				isxdigit(*(const unsigned char *)(src + i + 2))) {
				a = tolower(*(const unsigned char *)(src + i + 1));
				b = tolower(*(const unsigned char *)(src + i + 2));
				dst[j] = (char)((HEXTOI(a) << 4) | HEXTOI(b));
				i += 2;
			}
			else
			{
				return -1;
			}
		}
		else if (is_form_url_encoded && src[i] == '+')
		{
			dst[j] = ' ';
		}
		else
		{
			dst[j] = src[i];
		}
	}

	dst[j] = '\0'; /* Null-terminate the destination */

	return i >= src_len ? j : -1;
}

TsHttpRpc::TsHttpRpc()
{
	m_stop = false;
	mg_mgr_init(&m_mg_mgr, NULL);
}

TsHttpRpc::~TsHttpRpc()
{
	mg_mgr_free(&m_mg_mgr);
}

bool TsHttpRpc::init(const char* ip, int port)
{
	char file_name[MAX_PATH] = { 0 };
	if (!GetModuleFileNameA(NULL, file_name, MAX_PATH))
		return false;

	int len = strlen(file_name);

	if (file_name[len] == '\\')
	{
		file_name[len] = '\0';
	}
	char* match = strrchr(file_name, '\\');
	if (match != NULL)
	{
		*match = '\0';
	}

	struct mg_connection* nc = NULL;

	char addr[128] = { 0 };
	if (0 == strcmp(ip, "127.0.0.1") || 0 == strcmp(ip, "localhost"))
		ex_strformat(addr, 128, "tcp://127.0.0.1:%d", port);
	else
		ex_strformat(addr, 128, "tcp://%s:%d", ip, port);

	nc = mg_bind(&m_mg_mgr, addr, _mg_event_handler);
	if (nc == NULL)
	{
		EXLOGE("[rpc] TsHttpRpc::init %s:%d\n", ip, port);
		return false;
	}
	nc->user_data = this;

	mg_set_protocol_http_websocket(nc);

	m_content_type_map[".js"] = "application/javascript";
	m_content_type_map[".png"] = "image/png";
	m_content_type_map[".jpeg"] = "image/jpeg";
	m_content_type_map[".jpg"] = "image/jpeg";
	m_content_type_map[".gif"] = "image/gif";
	m_content_type_map[".ico"] = "image/x-icon";
	m_content_type_map[".json"] = "image/json";
	m_content_type_map[".html"] = "text/html";
	m_content_type_map[".css"] = "text/css";
	m_content_type_map[".tif"] = "image/tiff";
	m_content_type_map[".tiff"] = "image/tiff";
	m_content_type_map[".svg"] = "text/html";

	return true;
}

void TsHttpRpc::run(void)
{
	while(!m_stop)
	{
		mg_mgr_poll(&m_mg_mgr, 500);
	}
}

void TsHttpRpc::stop(void)
{
	m_stop = true;
}

void TsHttpRpc::_mg_event_handler(struct mg_connection *nc, int ev, void *ev_data)
{
	struct http_message *hm = (struct http_message*)ev_data;

	TsHttpRpc* _this = (TsHttpRpc*)nc->user_data;
	if (NULL == _this)
	{
		EXLOGE("[ERROR] invalid http request.\n");
		return;
	}

	switch (ev)
	{
	case MG_EV_HTTP_REQUEST:
	{
		ex_astr uri;
		ex_chars _uri;
		_uri.resize(hm->uri.len + 1);
		memset(&_uri[0], 0, hm->uri.len + 1);
		memcpy(&_uri[0], hm->uri.p, hm->uri.len);
		uri = &_uri[0];

#ifdef EX_DEBUG
		char* dbg_method = NULL;
		if (hm->method.len == 3 && 0 == memcmp(hm->method.p, "GET", hm->method.len))
			dbg_method = "GET";
		else if (hm->method.len == 4 && 0 == memcmp(hm->method.p, "POST", hm->method.len))
			dbg_method = "POST";
		else
			dbg_method = "UNSUPPORTED-HTTP-METHOD";

		EXLOGV("[rpc] got %s request: %s\n", dbg_method, uri.c_str());
#endif
		ex_astr ret_buf;
		bool b_is_index = false;

		if (uri == "/")
		{
			ex_wstr page = L"<html lang=\"zh_CN\"><head><meta charset=\"utf-8\"/><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/><title>Teleport����</title>\n<style type=\"text/css\">\n.box{padding:20px;margin:40px;border:1px solid #78b17c;background-color:#e4ffe5;}\n</style>\n</head><body><div class=\"box\">Teleport���ֹ���������</div></body></html>";
			ex_wstr2astr(page, ret_buf, EX_CODEPAGE_UTF8);

			mg_printf(nc, "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n%s", ret_buf.size() - 1, &ret_buf[0]);
			nc->flags |= MG_F_SEND_AND_CLOSE;
			return;
		}

		if (uri == "/config")
		{
			uri = "/index.html";
			b_is_index = true;
		}

		ex_astr temp;
		int offset = uri.find("/", 1);
		if (offset > 0)
		{
			temp = uri.substr(1, offset-1);

			if(temp == "api") {
				ex_astr method;
				ex_astr json_param;
				int rv = _this->_parse_request(hm, method, json_param);
				if (0 != rv)
				{
					EXLOGE("[ERROR] http-rpc got invalid request.\n");
					_this->_create_json_ret(ret_buf, rv);
				}
				else
				{
					_this->_process_js_request(method, json_param, ret_buf);
				}
				
				mg_printf(nc, "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %ld\r\nContent-Type: application/json\r\n\r\n%s", ret_buf.size() - 1, &ret_buf[0]);
				nc->flags |= MG_F_SEND_AND_CLOSE;
				return;
			}
		}

		
		ex_astr file_suffix;
		offset = uri.rfind(".");
		if (offset > 0)
		{
			file_suffix = uri.substr(offset, uri.length());
		}
		
		ex_wstr2astr(g_env.m_site_path, temp);
		ex_astr index_path = temp + uri;
		

		FILE* file = ex_fopen(index_path.c_str(), "rb");
		if (file)
		{
			unsigned long file_size = 0;
			char* buf = 0;
			size_t ret = 0;

			fseek(file, 0, SEEK_END);
			file_size = ftell(file);
			buf = new char[file_size];
			memset(buf, 0, file_size);
			fseek(file, 0, SEEK_SET);
			ret = fread(buf, 1, file_size, file);
			fclose(file);
			
			ex_astr content_type = _this->get_content_type(file_suffix);
			
			mg_printf(nc, "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %ld\r\nContent-Type: %s\r\n\r\n", file_size, content_type.c_str());
			mg_send(nc, buf, (int)file_size);
			delete []buf;
			nc->flags |= MG_F_SEND_AND_CLOSE;
			return;
		}
		else if (b_is_index)
		{
			ex_wstr page = L"<html lang=\"zh_CN\"><html><head><title>404 Not Found</title></head><body bgcolor=\"white\"><center><h1>404 Not Found</h1></center><hr><center><p>Teleport Assistor configuration page not found.</p></center></body></html>";
			ex_wstr2astr(page, ret_buf, EX_CODEPAGE_UTF8);
			
			mg_printf(nc, "HTTP/1.0 404 File Not Found\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: %ld\r\nContent-Type: text/html\r\n\r\n%s", ret_buf.size() - 1, &ret_buf[0]);
			nc->flags |= MG_F_SEND_AND_CLOSE;
			return;
		}
		
	}
	break;
	default:
		break;
	}
}

int TsHttpRpc::_parse_request(struct http_message* req, ex_astr& func_cmd, ex_astr& func_args)
{
	if (NULL == req)
		return TPE_FAILED;

	bool is_get = true;
	if (req->method.len == 3 && 0 == memcmp(req->method.p, "GET", req->method.len))
		is_get = true;
	else if (req->method.len == 4 && 0 == memcmp(req->method.p, "POST", req->method.len))
		is_get = false;
	else
		return TPE_HTTP_METHOD;

	ex_astrs strs;

	size_t pos_start = 1;	// ������һ���ֽڣ�һ���� '/'

	size_t i = 0;
	for (i = pos_start; i < req->uri.len; ++i)
	{
		if (req->uri.p[i] == '/')
		{
			if (i - pos_start > 0)
			{
				ex_astr tmp_uri;
				tmp_uri.assign(req->uri.p + pos_start, i - pos_start);
				strs.push_back(tmp_uri);
			}
			pos_start = i + 1;	// ������ǰ�ҵ��ķָ���
		}
	}
	if (pos_start < req->uri.len)
	{
		ex_astr tmp_uri;
		tmp_uri.assign(req->uri.p + pos_start, req->uri.len - pos_start);
		strs.push_back(tmp_uri);
	}

	if (0 == strs.size() || strs[0] != "api")
		return TPE_PARAM;

	if (is_get)
	{
		if (2 == strs.size())
		{
			func_cmd = strs[1];
		}
		else if (3 == strs.size())
		{
			func_cmd = strs[1];
			func_args = strs[2];
		}
		else
		{
			return TPE_PARAM;
		}
	}
	else
	{
		if (2 == strs.size())
		{
			func_cmd = strs[1];
		}
		else
		{
			return TPE_PARAM;
		}

		if (req->body.len > 0)
		{
			func_args.assign(req->body.p, req->body.len);
		}
	}

	if (func_args.length() > 0)
	{
		// ���������� url-decode ����
		int len = func_args.length() * 2;
		ex_chars sztmp;
		sztmp.resize(len);
		memset(&sztmp[0], 0, len);
		if (-1 == ts_url_decode(func_args.c_str(), func_args.length(), &sztmp[0], len, 0))
			return TPE_HTTP_URL_ENCODE;

		func_args = &sztmp[0];
	}

	EXLOGV("[rpc] method=%s, json_param=%s\n", func_cmd.c_str(), func_args.c_str());

	return TPE_OK;
}

void TsHttpRpc::_process_js_request(const ex_astr& func_cmd, const ex_astr& func_args, ex_astr& buf)
{
	if (func_cmd == "get_version")
	{
		_rpc_func_get_version(func_args, buf);
	}
	else if (func_cmd == "run")
	{
		_rpc_func_run_client(func_args, buf);
	}
// 	else if (func_cmd == "check")
// 	{
// 		_rpc_func_check(func_args, buf);
// 	}
	else if (func_cmd == "rdp_play")
	{
		_rpc_func_rdp_play(func_args, buf);
	}
	else if (func_cmd == "get_config")
	{
		_rpc_func_get_config(func_args, buf);
	}
	else if (func_cmd == "set_config")
	{
		_rpc_func_set_config(func_args, buf);
	}
	else if (func_cmd == "file_action")
	{
		_rpc_func_file_action(func_args, buf);
	}
	else
	{
		EXLOGE("[rpc] got unknown command: %s\n", func_cmd.c_str());
		_create_json_ret(buf, TPE_UNKNOWN_CMD);
	}
}

void TsHttpRpc::_create_json_ret(ex_astr& buf, int errcode)
{
	// ���أ� {"code":123}

	Json::FastWriter jr_writer;
	Json::Value jr_root;

	jr_root["code"] = errcode;
	buf = jr_writer.write(jr_root);
}

void TsHttpRpc::_create_json_ret(ex_astr& buf, Json::Value& jr_root)
{
	Json::FastWriter jr_writer;
	buf = jr_writer.write(jr_root);
}

void TsHttpRpc::_rpc_func_run_client(const ex_astr& func_args, ex_astr& buf)
{
	// ��Σ�{"ip":"192.168.5.11","port":22,"uname":"root","uauth":"abcdefg","authmode":1,"protocol":2}
	//   authmode: 1=password, 2=private-key
	//   protocol: 1=rdp, 2=ssh
	// SSH���أ� {"code":0, "data":{"sid":"0123abcde"}}
	// RDP���أ� {"code":0, "data":{"sid":"0123abcde0A"}}

	Json::Reader jreader;
	Json::Value jsRoot;

	if (!jreader.parse(func_args.c_str(), jsRoot))
	{
		_create_json_ret(buf, TPE_JSON_FORMAT);
		return;
	}
	if (!jsRoot.isObject())
	{
		_create_json_ret(buf, TPE_PARAM);
		return;
	}

	// �жϲ����Ƿ���ȷ
	if (!jsRoot["teleport_ip"].isString() || !jsRoot["size"].isNumeric()
		|| !jsRoot["teleport_port"].isNumeric() || !jsRoot["remote_host_ip"].isString()
		|| !jsRoot["session_id"].isString() || !jsRoot["protocol_type"].isNumeric() || !jsRoot["protocol_sub_type"].isNumeric()
		)
	{
		_create_json_ret(buf, TPE_PARAM);
		return;
	}

	int pro_sub = jsRoot["protocol_sub_type"].asInt();

	ex_astr teleport_ip = jsRoot["teleport_ip"].asCString();
	int teleport_port = jsRoot["teleport_port"].asUInt();

	int windows_size = 2;
	if (jsRoot["size"].isNull())
		windows_size = 2;
	else
		windows_size = jsRoot["size"].asUInt();

	int console = 0;
	if (jsRoot["console"].isNull())
		console = 0;
	else
		console = jsRoot["console"].asUInt();

	ex_astr real_host_ip = jsRoot["remote_host_ip"].asCString();
	ex_astr sid = jsRoot["session_id"].asCString();

	int pro_type = jsRoot["protocol_type"].asUInt();

	ex_wstr w_exe_path;
	WCHAR w_szCommandLine[MAX_PATH] = { 0 };


	ex_wstr w_sid;
	ex_astr2wstr(sid, w_sid);
	ex_wstr w_teleport_ip;
	ex_astr2wstr(teleport_ip, w_teleport_ip);
	ex_wstr w_real_host_ip;
	ex_astr2wstr(real_host_ip, w_real_host_ip);
	WCHAR w_port[32] = { 0 };
	swprintf_s(w_port, _T("%d"), teleport_port);

	ex_wstr tmp_rdp_file; // for .rdp file

	if (pro_type == TP_PROTOCOL_TYPE_RDP)
	{
		//==============================================
		// RDP
		//==============================================
#if 0

#if defined(RDP_CLIENT_SYSTEM_ACTIVE_CONTROL)
		int split_pos = session_id.length() - 2;
		std::string real_s_id = session_id.substr(0, split_pos);
		std::string str_pwd_len = session_id.substr(split_pos, session_id.length());
		int n_pwd_len = strtol(str_pwd_len.c_str(), NULL, 16);
		n_pwd_len -= real_s_id.length();
		WCHAR w_szPwd[256] = { 0 };
		for (int i = 0; i < n_pwd_len; i++)
		{
			w_szPwd[i] = '*';
		}

		w_exe_path = g_env.m_tools_path + _T("\\tprdp\\tp_rdp.exe");
		ex_wstr w_s_id;
		ex_astr2str(real_s_id, w_s_id);
		ex_wstr w_server_ip;
		ex_astr2str(server_ip, w_server_ip);

		ex_wstr w_host_ip;
		ex_astr2str(host_ip, w_host_ip);

		swprintf_s(w_szCommandLine, _T(" -h%s -u%s -p%s -x%d -d%s -r%d"), w_server_ip.c_str(), w_s_id.c_str(), w_szPwd, host_port, w_host_ip.c_str(), windows_size);

		// 		sprintf_s(sz_file_name, ("%s\\%s.rdp"), temp_path, temp_host_ip.c_str());
		// 		FILE* f = fopen(sz_file_name, ("wt"));
		// 		if (f == NULL)
		// 		{
		// 			printf("fopen failed (%d).\n", GetLastError());
		// 			_create_json_ret(buf, TSR_OPENFILE_ERROR);
		// 			return;
		// 		}
		// 		// Write a string into the file.
		// 		fwrite(sz_rdp_file_content, strlen(sz_rdp_file_content), 1, f);
		// 		fclose(f);
		// 		ex_wstr w_sz_file_name;
		// 		ex_astr2str(sz_file_name, w_sz_file_name);
		//		swprintf_s(w_szCommandLine, _T("mstsc %s"), w_sz_file_name.c_str());

		w_exe_path += w_szCommandLine;
		//BOOL bRet = DeleteFile(w_sz_file_name.c_str());
#elif defined(RDP_CLIENT_FREERDP)
		wchar_t* w_screen = NULL;

		switch (windows_size)
		{
		case 0: //ȫ��
			w_screen = _T("/f");
			break;
		case 2:
			w_screen = _T("/size:1024x768");
			break;
		case 3:
			w_screen = _T("/size:1280x1024");
			break;
		case 1:
		default:
			w_screen = _T("/size:800x600");
			break;
		}

		int split_pos = sid.length() - 2;
		std::string real_sid = sid.substr(0, split_pos);
		std::string str_pwd_len = sid.substr(split_pos, sid.length());
		int n_pwd_len = strtol(str_pwd_len.c_str(), NULL, 16);
		n_pwd_len -= real_sid.length();
		WCHAR w_szPwd[256] = { 0 };
		for (int i = 0; i < n_pwd_len; i++)
		{
			w_szPwd[i] = '*';
		}

		ex_astr2wstr(real_sid, w_sid);


		w_exe_path = _T("\"");
		w_exe_path += g_env.m_tools_path + _T("\\tprdp\\tprdp-client.exe\"");

		// use /gdi:sw otherwise the display will be yellow.
		if (console != 0)
		{
			swprintf_s(w_szCommandLine, _T(" %s /v:{host_ip}:{host_port} /admin /u:{user_name} /p:%s +clipboard /drives /gdi:sw /t:\"TP#{real_ip}\""),
				w_screen, w_szPwd
				);
		}
		else
		{
			swprintf_s(w_szCommandLine, _T(" %s /v:{host_ip}:{host_port} /u:{user_name} /p:%s +clipboard /drives /gdi:sw /t:\"TP#{real_ip}\""),
				w_screen, w_szPwd
				);
		}

		w_exe_path += w_szCommandLine;


#elif defined(RDP_CLIENT_SYSTEM_BUILTIN)
		int width = 800;
		int higth = 600;
		int cx = 0;
		int cy = 0;

		int display = 1;
		int iWidth = GetSystemMetrics(SM_CXSCREEN);
		int iHeight = GetSystemMetrics(SM_CYSCREEN);
		switch (windows_size)
		{
		case 0:
			//ȫ��
			width = iWidth;
			higth = iHeight;
			display = 2;
			break;
		case 1:
		{
			width = 800;
			higth = 600;
			display = 1;
			break;
		}
		case 2:
		{
			width = 1024;
			higth = 768;
			display = 1;
			break;
		}
		case 3:
		{
			width = 1280;
			higth = 1024;
			display = 1;
			break;
		}
		default:
			//int iWidth = GetSystemMetrics(SM_CXSCREEN);
			//int iHeight = GetSystemMetrics(SM_CYSCREEN);
			//width = iWidth;
			//width = iHeight - 50;
			width = 800;
			higth = 600;
			break;
		}

		cx = (iWidth - width) / 2;
		cy = (iHeight - higth) / 2;
		if (cx < 0)
		{
			cx = 0;
		}
		if (cy < 0)
		{
			cy = 0;
		}

		int split_pos = sid.length() - 2;
		std::string real_sid = sid.substr(0, split_pos);

// 		std::string psw51b;
// 		if (!calc_psw51b("Abcd1234", psw51b))
// 		{
// 			printf("calc password failed.\n");
// 			_create_json_ret(buf, TPE_FAILED);
// 			return;
// 		}

		char sz_rdp_file_content[4096] = { 0 };
		sprintf_s(sz_rdp_file_content, rdp_content.c_str(),
			console, display, width, higth
			, cx, cy, cx + width + 20, cy + higth + 40
			, teleport_ip.c_str(), teleport_port
			, real_sid.c_str()
//			, "administrator"
//			, psw51b.c_str()
			);

		char sz_file_name[MAX_PATH] = { 0 };
		char temp_path[MAX_PATH] = { 0 };
		DWORD ret = GetTempPathA(MAX_PATH, temp_path);
		if (ret <= 0)
		{
			printf("fopen failed (%d).\n", GetLastError());
			_create_json_ret(buf, TPE_FAILED);
			return;
		}
		ex_wstr w_s_id;
		ex_astr2wstr(real_sid, w_s_id);

		ex_astr temp_host_ip = real_host_ip;// replace_all_distinct(real_host_ip, ("."), "-");
		ex_replace_all(temp_host_ip, ".", "-");

		// for debug
		//sprintf_s(sz_file_name, ("e:\\tmp\\rdp\\%s.rdp"), temp_host_ip.c_str());

		sprintf_s(sz_file_name, ("%s%s.rdp"), temp_path, temp_host_ip.c_str());

		FILE* f = NULL;
		if(fopen_s(&f, sz_file_name, "wt") != 0)
		{
			printf("fopen failed (%d).\n", GetLastError());
			_create_json_ret(buf, TPE_OPENFILE);
			return;
		}
		// Write a string into the file.
		fwrite(sz_rdp_file_content, strlen(sz_rdp_file_content), 1, f);
		fclose(f);
		ex_wstr w_sz_file_name;
		ex_astr2wstr(sz_file_name, w_sz_file_name);

		swprintf_s(w_szCommandLine, _T("mstsc \"%s\""), w_sz_file_name.c_str());
		w_exe_path = w_szCommandLine;
		//BOOL bRet = DeleteFile(w_sz_file_name.c_str());
#endif
#endif

		w_exe_path = _T("\"");
		w_exe_path += g_cfg.rdp_app + _T("\" ");
		w_exe_path += g_cfg.rdp_cmdline;

		ex_wstr rdp_name = g_cfg.rdp_name;
		if (rdp_name == L"mstsc") {
			int width = 800;
			int higth = 600;
			int cx = 0;
			int cy = 0;

			int display = 1;
			int iWidth = GetSystemMetrics(SM_CXSCREEN);
			int iHeight = GetSystemMetrics(SM_CYSCREEN);
			switch (windows_size)
			{
			case 0:
				//ȫ��
				width = iWidth;
				higth = iHeight;
				display = 2;
				break;
			case 1:
			{
				width = 800;
				higth = 600;
				display = 1;
				break;
			}
			case 2:
			{
				width = 1024;
				higth = 768;
				display = 1;
				break;
			}
			case 3:
			{
				width = 1280;
				higth = 1024;
				display = 1;
				break;
			}
			default:
				width = 800;
				higth = 600;
				break;
			}

			cx = (iWidth - width) / 2;
			cy = (iHeight - higth) / 2;
			if (cx < 0)
			{
				cx = 0;
			}
			if (cy < 0)
			{
				cy = 0;
			}

			int split_pos = sid.length() - 2;
			std::string real_sid = sid.substr(0, split_pos);

			char sz_rdp_file_content[4096] = { 0 };
			sprintf_s(sz_rdp_file_content, rdp_content.c_str(),
				console, display, width, higth
				, cx, cy, cx + width + 20, cy + higth + 40
				, teleport_ip.c_str(), teleport_port
				, real_sid.c_str()
				);

			char sz_file_name[MAX_PATH] = { 0 };
			char temp_path[MAX_PATH] = { 0 };
			DWORD ret = GetTempPathA(MAX_PATH, temp_path);
			if (ret <= 0)
			{
				printf("fopen failed (%d).\n", GetLastError());
				_create_json_ret(buf, TPE_FAILED);
				return;
			}
			ex_wstr w_s_id;
			ex_astr2wstr(real_sid, w_s_id);

			ex_astr temp_host_ip = real_host_ip;// replace_all_distinct(real_host_ip, ("."), "-");
			ex_replace_all(temp_host_ip, ".", "-");

			sprintf_s(sz_file_name, ("%s%s.rdp"), temp_path, temp_host_ip.c_str());

			FILE* f = NULL;
			if (fopen_s(&f, sz_file_name, "wt") != 0)
			{
				printf("fopen failed (%d).\n", GetLastError());
				_create_json_ret(buf, TPE_OPENFILE);
				return;
			}
			// Write a string into the file.
			fwrite(sz_rdp_file_content, strlen(sz_rdp_file_content), 1, f);
			fclose(f);
			ex_astr2wstr(sz_file_name, tmp_rdp_file);

			// �����滻
			ex_replace_all(w_exe_path, _T("{tmp_rdp_file}"), tmp_rdp_file);
		}
		else if (g_cfg.rdp_name == L"freerdp") {
			wchar_t* w_screen = NULL;

			switch (windows_size)
			{
			case 0: //ȫ��
				w_screen = _T("/f");
				break;
			case 2:
				w_screen = _T("/size:1024x768");
				break;
			case 3:
				w_screen = _T("/size:1280x1024");
				break;
			case 1:
			default:
				w_screen = _T("/size:800x600");
				break;
			}

			wchar_t* w_console = NULL;

			if (console != 0)
			{
				w_console = L"/admin";
			}
			else
			{
				w_console = L"";
			}

			int split_pos = sid.length() - 2;
			std::string real_sid = sid.substr(0, split_pos);
			std::string str_pwd_len = sid.substr(split_pos, sid.length());
			int n_pwd_len = strtol(str_pwd_len.c_str(), NULL, 16);
			n_pwd_len -= real_sid.length();
			WCHAR w_szPwd[256] = { 0 };
			for (int i = 0; i < n_pwd_len; i++)
			{
				w_szPwd[i] = '*';
			}

			ex_astr2wstr(real_sid, w_sid);

			// �����滻
			ex_replace_all(w_exe_path, _T("{size}"), w_screen);
			ex_replace_all(w_exe_path, _T("{console}"), w_console);
			ex_replace_all(w_exe_path, _T("{clipboard}"), L"+clipboard");
			ex_replace_all(w_exe_path, _T("{drives}"), L"/drives");
		}
		else {
			_create_json_ret(buf, TPE_FAILED);
			return;
		}


	}
	else if (pro_type == TP_PROTOCOL_TYPE_SSH)
	{
		//==============================================
		// SSH
		//==============================================

		if (pro_sub == TP_PROTOCOL_TYPE_SSH_SHELL)
		{
			w_exe_path = _T("\"");
			w_exe_path += g_cfg.ssh_app + _T("\" ");
			w_exe_path += g_cfg.ssh_cmdline;
		}
		else
		{
			w_exe_path = _T("\"");
			w_exe_path += g_cfg.scp_app + _T("\" ");
			w_exe_path += g_cfg.scp_cmdline;
		}
	}
	else if (pro_type == TP_PROTOCOL_TYPE_TELNET)
	{
		//==============================================
		// TELNET
		//==============================================
		w_exe_path = _T("\"");
		w_exe_path += g_cfg.telnet_app + _T("\" ");
		w_exe_path += g_cfg.telnet_cmdline;
	}

	ex_replace_all(w_exe_path, _T("{host_port}"), w_port);
	ex_replace_all(w_exe_path, _T("{host_ip}"), w_teleport_ip.c_str());
	ex_replace_all(w_exe_path, _T("{user_name}"), w_sid.c_str());
	ex_replace_all(w_exe_path, _T("{real_ip}"), w_real_host_ip.c_str());
	ex_replace_all(w_exe_path, _T("{assist_tools_path}"), g_env.m_tools_path.c_str());

	
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	Json::Value root_ret;
	ex_astr utf8_path;
	ex_wstr2astr(w_exe_path, utf8_path, EX_CODEPAGE_UTF8);
	root_ret["path"] = utf8_path;

	if (!CreateProcess(NULL, (wchar_t *)w_exe_path.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		EXLOGE(_T("CreateProcess() failed. Error=0x%08X.\n  %s\n"), GetLastError(), w_exe_path.c_str());
		root_ret["code"] = TPE_START_CLIENT;
		_create_json_ret(buf, root_ret);
		return;
	}

	root_ret["code"] = TPE_OK;
	_create_json_ret(buf, root_ret);
}

// bool isIPAddress(const char *s)
// {
// 	const char *pChar;
// 	bool rv = true;
// 	int tmp1, tmp2, tmp3, tmp4, i;
// 	while (1)
// 	{
// 		i = sscanf_s(s, "%d.%d.%d.%d", &tmp1, &tmp2, &tmp3, &tmp4);
// 		if (i != 4)
// 		{
// 			rv = false;
// 			break;
// 		}
// 
// 		if ((tmp1 > 255) || (tmp2 > 255) || (tmp3 > 255) || (tmp4 > 255))
// 		{
// 			rv = false;
// 			break;
// 		}
// 
// 		for (pChar = s; *pChar != 0; pChar++)
// 		{
// 			if ((*pChar != '.')
// 				&& ((*pChar < '0') || (*pChar > '9')))
// 			{
// 				rv = false;
// 				break;
// 			}
// 		}
// 		break;
// 	}
// 
// 	return rv;
// }
// 
// void TsHttpRpc::_rpc_func_check(const ex_astr& func_args, ex_astr& buf)
// {
// 	// ��Σ�{"ip":"192.168.5.11","port":22,"uname":"root","uauth":"abcdefg","authmode":1,"protocol":2}
// 	//   authmode: 1=password, 2=private-key
// 	//   protocol: 1=rdp, 2=ssh
// 	// SSH���أ� {"code":0, "data":{"sid":"0123abcde"}}
// 	// RDP���أ� {"code":0, "data":{"sid":"0123abcde0A"}}
// 
// 	Json::Reader jreader;
// 	Json::Value jsRoot;
// 
// 	if (!jreader.parse(func_args.c_str(), jsRoot))
// 	{
// 		_create_json_ret(buf, TPE_JSON_FORMAT);
// 		return;
// 	}
// 	if (jsRoot.isArray())
// 	{
// 		_create_json_ret(buf, TPE_PARAM);
// 		return;
// 	}
// 	int windows_size = 2;
// 
// 
// 
// 	// �жϲ����Ƿ���ȷ
// 	if (!jsRoot["server_ip"].isString() || !jsRoot["ssh_port"].isNumeric()
// 		|| !jsRoot["rdp_port"].isNumeric()
// 		)
// 	{
// 		_create_json_ret(buf, TPE_PARAM);
// 		return;
// 	}
// 
// 	std::string host = jsRoot["server_ip"].asCString();
// 	int rdp_port = jsRoot["rdp_port"].asUInt();
// 	int ssh_port = jsRoot["rdp_port"].asUInt();
// 	std::string server_ip;
// 	if (isIPAddress(host.c_str()))
// 	{
// 		server_ip = host;
// 	}
// 	else
// 	{
// 		char *ptr, **pptr;
// 		struct hostent *hptr;
// 		char IP[128] = { 0 };
// 		/* ȡ��������һ����������Ҫ������������������ */
// 		ptr = (char*)host.c_str();
// 		/* ����gethostbyname()�����ý��������hptr�� */
// 		if ((hptr = gethostbyname(ptr)) == NULL)
// 		{
// 			//printf("gethostbyname error for host:%s/n", ptr);
// 			_create_json_ret(buf, TPE_PARAM);
// 			return;
// 		}
// 
// 		char szbuf[1204] = { 0 };
// 		switch (hptr->h_addrtype)
// 		{
// 		case AF_INET:
// 		case AF_INET6:
// 			pptr = hptr->h_addr_list;
// 			for (; *pptr != NULL; pptr++)
// 				ex_inet_ntop(hptr->h_addrtype, *pptr, IP, sizeof(IP));
// 			server_ip = IP;
// 			break;
// 		default:
// 			printf("unknown address type/n");
// 			break;
// 		}
// 	}
// 	if (!isIPAddress(server_ip.c_str()))
// 	{
// 		_create_json_ret(buf, TPE_PARAM);
// 		return;
// 	}
// 	if (TestTCPPort(server_ip, rdp_port) && TestTCPPort(server_ip, ssh_port))
// 	{
// 		_create_json_ret(buf, TPE_OK);
// 		return;
// 	}
// 	ICMPheaderRet temp = { 0 };
// 	int b_ok = ICMPSendTo(&temp, (char*)server_ip.c_str(), 16, 8);
// 	if (b_ok == 0)
// 	{
// 		_create_json_ret(buf, TPE_OK);
// 		return;
// 	}
// 	else
// 	{
// 		_create_json_ret(buf, TPE_NETWORK);
// 	}
// 
// 	return;
// }

void TsHttpRpc::_rpc_func_rdp_play(const ex_astr& func_args, ex_astr& buf)
{
	Json::Reader jreader;
	Json::Value jsRoot;

	if (!jreader.parse(func_args.c_str(), jsRoot))
	{
		_create_json_ret(buf, TPE_JSON_FORMAT);
		return;
	}

	// �жϲ����Ƿ���ȷ
// 	if (!jsRoot["host"].isString())
// 	{
// 		_create_json_ret(buf, TPE_PARAM);
// 		return;
// 	}
// 	if (!jsRoot["port"].isInt())
// 	{
// 		_create_json_ret(buf, TPE_PARAM);
// 		return;
// 	}

	if (!jsRoot["rid"].isInt()
		|| !jsRoot["web"].isString()
		|| !jsRoot["sid"].isString()
		|| !jsRoot["user"].isString()
		|| !jsRoot["acc"].isString()
		|| !jsRoot["host"].isString()
		|| !jsRoot["start"].isString()
		)
	{
		_create_json_ret(buf, TPE_PARAM);
		return;
	}



// 	if (!jsRoot["tail"].isString())
// 	{
// 		_create_json_ret(buf, TPE_PARAM);
// 		return;
// 	}

	int rid = jsRoot["rid"].asInt();
	ex_astr a_url_base = jsRoot["web"].asCString();
	ex_astr a_sid = jsRoot["sid"].asCString();
	ex_astr a_user = jsRoot["user"].asCString();
	ex_astr a_acc = jsRoot["acc"].asCString();
	ex_astr a_host = jsRoot["host"].asCString();
	ex_astr a_start = jsRoot["start"].asCString();
	//ex_astr a_tail = jsRoot["tail"].asCString();

	char cmd_args[1024] = { 0 };
	ex_strformat(cmd_args, 1023, "%d \"%s\" \"%09d-%s-%s-%s-%s\"", rid, a_sid.c_str(), rid, a_user.c_str(), a_acc.c_str(), a_host.c_str(), a_start.c_str());


// 	ex_astr a_host = jsRoot["host"].asCString();
// 	int port = jsRoot["port"].asInt();
// 	ex_astr a_tail = jsRoot["tail"].asCString();
// 	ex_astr server_ip;
// 	if (isIPAddress(a_host.c_str()))
// 	{
// 		server_ip = a_host;
// 	}
// 	else
// 	{
// 		char *ptr, **pptr;
// 		struct hostent *hptr;
// 		char IP[128] = { 0 };
// 		// ȡ��������һ����������Ҫ������������������
// 		ptr = (char*)a_host.c_str();
// 		// ����gethostbyname()�����ý��������hptr��
// 		if ((hptr = gethostbyname(ptr)) == NULL)
// 		{
// 			//printf("gethostbyname error for host:%s/n", ptr);
// 			_create_json_ret(buf, TPE_PARAM);
// 			return;
// 		}
// 		// �������Ĺ淶�������
// 		//printf("official hostname:%s/n", hptr->h_name);
// 		// ���������ж�������������б����ֱ�����
// 		//for (pptr = hptr->h_aliases; *pptr != NULL; pptr++)
// 		//	printf(" alias:%s/n", *pptr);
// 		// ���ݵ�ַ���ͣ�����ַ�����
// 		char szbuf[1204] = { 0 };
// 		switch (hptr->h_addrtype)
// 		{
// 		case AF_INET:
// 		case AF_INET6:
// 			pptr = hptr->h_addr_list;
// 			// ���ղŵõ������е�ַ������������е�����inet_ntop()����
// 
// 			for (; *pptr != NULL; pptr++)
// 				inet_ntop(hptr->h_addrtype, *pptr, IP, sizeof(IP));
// 			server_ip = IP;
// 			break;
// 		default:
// 			printf("unknown address type/n");
// 			break;
// 		}
// 	}

// 	char szURL[256] = { 0 };
// 	sprintf_s(szURL, 256, "http://%s:%d/%s", server_ip.c_str(), port, a_tail.c_str());
// 	ex_astr a_url = szURL;
	ex_wstr w_url_base;
	ex_astr2wstr(a_url_base, w_url_base);
	ex_wstr w_cmd_args;
	ex_astr2wstr(cmd_args, w_cmd_args);

// 	char szHost[256] = { 0 };
// 	sprintf_s(szHost, 256, "%s:%d", a_host.c_str(), port);
// 
// 	a_host = szHost;
// 	ex_wstr w_host;
// 	ex_astr2wstr(a_host, w_host);
	
	ex_wstr w_exe_path;
	w_exe_path = _T("\"");
	w_exe_path += g_env.m_tools_path + _T("\\tprdp\\tprdp-replay.exe\"");
	w_exe_path += _T(" \"");
	w_exe_path += w_url_base;
	w_exe_path += _T("\" ");
	w_exe_path += w_cmd_args;

	Json::Value root_ret;
	ex_astr utf8_path;
	ex_wstr2astr(w_exe_path, utf8_path, EX_CODEPAGE_UTF8);
	root_ret["cmdline"] = utf8_path;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcess(NULL, (wchar_t *)w_exe_path.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		EXLOGE(_T("CreateProcess() failed. Error=0x%08X.\n  %s\n"), GetLastError(), w_exe_path.c_str());
		root_ret["code"] = TPE_START_CLIENT;
		_create_json_ret(buf, root_ret);
		return;
	}

	root_ret["code"] = TPE_OK;
	_create_json_ret(buf, root_ret);
	return;
}

void TsHttpRpc::_rpc_func_get_config(const ex_astr& func_args, ex_astr& buf)
{
	Json::Value jr_root;
	jr_root["code"] = 0;
	jr_root["data"] = g_cfg.get_root();
	_create_json_ret(buf, jr_root);
}

void TsHttpRpc::_rpc_func_set_config(const ex_astr& func_args, ex_astr& buf)
{
	Json::Reader jreader;
	Json::Value jsRoot;
	if (!jreader.parse(func_args.c_str(), jsRoot))
	{
		_create_json_ret(buf, TPE_JSON_FORMAT);
		return;
	}

	if(!g_cfg.save(func_args))
		_create_json_ret(buf, TPE_FAILED);
	else
		_create_json_ret(buf, TPE_OK);
}

void TsHttpRpc::_rpc_func_file_action(const ex_astr& func_args, ex_astr& buf) {

	Json::Reader jreader;
	Json::Value jsRoot;

	if (!jreader.parse(func_args.c_str(), jsRoot))
	{
		_create_json_ret(buf, TPE_JSON_FORMAT);
		return;
	}
	// �жϲ����Ƿ���ȷ
	if (!jsRoot["action"].isNumeric())
	{
		_create_json_ret(buf, TPE_PARAM);
		return;
	}
	int action = jsRoot["action"].asUInt();

	HWND hParent = GetForegroundWindow();
	if (NULL == hParent)
		hParent = g_hDlgMain;

	BOOL ret = FALSE;
	wchar_t wszReturnPath[MAX_PATH] = _T("");

	if (action == 1 || action == 2)
	{
		OPENFILENAME ofn;
		ex_wstr wsDefaultName;
		ex_wstr wsDefaultPath;
		StringCchCopy(wszReturnPath, MAX_PATH, wsDefaultName.c_str());

		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrTitle = _T("ѡ���ļ�");
		ofn.hwndOwner = hParent;
		ofn.lpstrFilter = _T("��ִ�г��� (*.exe)\0*.exe\0");
		ofn.lpstrFile = wszReturnPath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = wsDefaultPath.c_str();
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST;

		if (action == 1)
		{
			ofn.Flags |= OFN_FILEMUSTEXIST;
			ret = GetOpenFileName(&ofn);
		}
		else
		{
			ofn.Flags |= OFN_OVERWRITEPROMPT;
			ret = GetSaveFileName(&ofn);
		}
	}
	else if (action == 3)
	{
		BROWSEINFO bi;
		ZeroMemory(&bi, sizeof(BROWSEINFO));
		bi.hwndOwner = NULL;
		bi.pidlRoot = NULL;
		bi.pszDisplayName = wszReturnPath; //�˲�����ΪNULL������ʾ�Ի���
		bi.lpszTitle = _T("ѡ��Ŀ¼");
		bi.ulFlags = BIF_RETURNONLYFSDIRS;
		bi.lpfn = NULL;
		bi.iImage = 0;   //��ʼ����ڲ���bi����
		LPITEMIDLIST pIDList = SHBrowseForFolder(&bi);//������ʾѡ��Ի���
		if (pIDList)
		{
			ret = true;
			SHGetPathFromIDList(pIDList, wszReturnPath);
		}
		else
		{
			ret = false;
		}
	}
	else if (action == 4)
	{
		ex_wstr wsDefaultName;
		ex_wstr wsDefaultPath;

		if (wsDefaultPath.length() == 0)
		{
			_create_json_ret(buf, TPE_PARAM);
			return;
		}

		ex_wstr::size_type pos = 0;

		while (ex_wstr::npos != (pos = wsDefaultPath.find(L"/", pos)))
		{
			wsDefaultPath.replace(pos, 1, L"\\");
			pos += 1;
		}

		ex_wstr wArg = L"/select, \"";
		wArg += wsDefaultPath;
		wArg += L"\"";
		if ((int)ShellExecute(hParent, _T("open"), _T("explorer"), wArg.c_str(), NULL, SW_SHOW) > 32)
			ret = true;
		else
			ret = false;
	}

	if (ret)
	{
		if (action == 1 || action == 2 || action == 3)
		{
			ex_astr utf8_path;
			ex_wstr2astr(wszReturnPath, utf8_path, EX_CODEPAGE_UTF8);
			Json::Value root;
			root["code"] = TPE_OK;
			root["path"] = utf8_path;
			_create_json_ret(buf, root);

			return;
		}
		else
		{
			_create_json_ret(buf, TPE_OK);
			return;
		}
	}
	else
	{
		_create_json_ret(buf, TPE_DATA);
		return;
	}
}

void TsHttpRpc::_rpc_func_get_version(const ex_astr& func_args, ex_astr& buf)
{
	Json::Value root_ret;
	ex_wstr w_version = TP_ASSIST_VER;
	ex_astr version;
	ex_wstr2astr(w_version, version, EX_CODEPAGE_UTF8);
	root_ret["version"] = version;
	root_ret["code"] = TPE_OK;
	_create_json_ret(buf, root_ret);
	return;
}