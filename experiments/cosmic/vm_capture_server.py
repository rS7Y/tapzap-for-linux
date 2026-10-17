import http.server,pathlib,re
root=pathlib.Path('/home/foton/TAPZAP-POP-20261017/captures')
class Capture(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  if not re.fullmatch(r'/resume-[a-f0-9]{32}-[0-9]+-[01]\.ppm',self.path):self.send_error(404);return
  try:data=(root/self.path[1:]).read_bytes()
  except FileNotFoundError:self.send_error(404);return
  self.send_response(200);self.send_header('Content-Length',str(len(data)));self.send_header('Content-Type','image/x-portable-pixmap');self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(data)
http.server.ThreadingHTTPServer(('127.0.0.1',22252),Capture).serve_forever()
