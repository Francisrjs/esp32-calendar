const fs=require('fs'),path=require('path'),http=require('http');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'C:/Users/franr/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=__dirname,server=http.createServer((req,res)=>{let url;try{url=decodeURIComponent(req.url.split('?')[0])}catch{res.writeHead(400);res.end();return}const file=path.resolve(root,'.'+(url==='/'?'/index.html':url));if(!file.startsWith(root+path.sep)){res.writeHead(403);res.end();return}fs.readFile(file,(err,data)=>{if(err){res.writeHead(404);res.end();return}res.setHeader('Content-Type',({'html':'text/html','js':'text/javascript','css':'text/css','png':'image/png','json':'application/json'})[file.split('.').pop()]||'text/plain');res.end(data)})});
async function start(){await new Promise(r=>server.listen(0,'127.0.0.1',r));return `http://127.0.0.1:${server.address().port}`}
module.exports={root,server,start,chromium};
