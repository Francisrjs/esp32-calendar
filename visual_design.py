"""Antialiased presentation components; hardware remains 400x300 monochrome."""
from PIL import Image, ImageDraw, ImageFont

S = 3

def refine_view(name, source):
    image = source.resize((400*S, 300*S), Image.Resampling.LANCZOS)
    d = ImageDraw.Draw(image)
    def box(bounds, radius=10, fill='white', outline='#c5cbc7', width=1):
        d.rounded_rectangle(tuple(int(v*S) for v in bounds), radius=radius*S, fill=fill, outline=outline, width=width*S)
    def text(x,y,label,size=12,bold=False,fill='#171e1a',anchor=None):
        font=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf' if bold else 'C:/Windows/Fonts/segoeui.ttf',size*S)
        d.text((x*S,y*S),label,font=font,fill=fill,anchor=anchor)
    def line(points,width=2,fill='#171e1a'):
        d.line([(x*S,y*S) for x,y in points],fill=fill,width=width*S,joint='curve')
    def icon(kind,x,y,size=20):
        def p(a,b): return (x+a*size,y+b*size)
        if kind=='home':
            line([p(.05,.45),p(.5,.05),p(.95,.45)])
            line([p(.2,.36),p(.2,.95),p(.8,.95),p(.8,.36)])
            line([p(.4,.95),p(.4,.65),p(.6,.65),p(.6,.95)])
        elif kind=='bell':
            line([p(.15,.75),p(.25,.6),p(.25,.3),p(.35,.15),p(.65,.15),p(.75,.3),p(.75,.6),p(.85,.75),p(.15,.75)])
            line([p(.4,.9),p(.6,.9)])
        elif kind=='plus':
            line([p(.5,.15),p(.5,.85)]); line([p(.15,.5),p(.85,.5)])
        elif kind=='play':
            d.polygon([tuple(int(v*S) for v in p(a,b)) for a,b in [(0.3,.15),(.85,.5),(.3,.85)]],fill='#171e1a')
        elif kind=='pause':
            line([p(.35,.18),p(.35,.82)],3);line([p(.65,.18),p(.65,.82)],3)
        elif kind in ('next','previous'):
            flip=kind=='previous'
            q=lambda a,b:p(1-a if flip else a,b)
            d.polygon([tuple(int(v*S) for v in q(a,b)) for a,b in [(.15,.2),(.7,.5),(.15,.8)]],fill='#171e1a')
            line([q(.8,.2),q(.8,.8)])
        else:
            box((x+2,y+3,x+size-2,y+size-2),4,outline='#171e1a')
            line([p(.2,.35),p(.8,.35)],1)

    if name in ('alarms','tasks','calendar','music'):
        d.rectangle((0,0,400*S,300*S),fill='#f5f7f5')
    d.rectangle((0,0,400*S,32*S),fill='white')
    icon('home',12,7,20)
    text(288,9,'22°C',11,bold=True)
    line([(345,9),(341,15),(341,18),(345,20),(349,18),(349,15),(345,9)],1)
    text(354,9,'45%',11,bold=True)
    if name=='home':
        d.rectangle((0,32*S,400*S,300*S),fill='white')
        image.paste(source.crop((0,269,400,300)).resize((400*S,31*S),Image.Resampling.LANCZOS),(0,269*S))
        d.rectangle((120*S,269*S,280*S,300*S),fill='white')
        for x,title,sub in [(10,'Alarmas','2 activas'),(295,'Tareas','2 pendientes')]:
            box((x,36,x+95,101),9)
            text(x+10,45,title,15,True);text(x+10,74,sub,12)
        portrait=source.crop((129,63,271,205)).resize((190*S,190*S),Image.Resampling.LANCZOS)
        mask=Image.new('L',(190*S,190*S),0)
        ImageDraw.Draw(mask).ellipse((2*S,2*S,188*S,188*S),fill=255)
        image.paste(portrait,(105*S,55*S),mask)
        text(200,242,'07:30',32,True,anchor='ma')
        text(200,276,'MIÉRCOLES · 25 OCTUBRE',9,True,anchor='ma')
        line([(195,295),(200,291),(205,295)],1)
    elif name=='alarms':
        text(16,38,'Alarmas',22,True);text(384,47,'2 horarios',11,anchor='ra',fill='#69736c')
        for i,(time,label,days) in enumerate([('07:00','Morning Routine','L  M  M  J  V'),('08:30','Weekend Backup','S  D')]):
            y=76+i*102
            box((16,y,384,y+92))
            icon('bell',28,y+16,22);text(62,y+5,time,30,True)
            text(30,y+51,label,13,True);text(30,y+71,days,10,fill='#69736c')
            box((328,y+17,369,y+39),11,fill='#171e1a' if i==0 else '#d4dcd6',outline=None)
            cx=357 if i==0 else 340
            d.ellipse(((cx-8)*S,(y+20)*S,(cx+8)*S,(y+36)*S),fill='white')
        box((349,259,384,294),17,fill='white');icon('plus',357,267,19)
    elif name=='tasks':
        text(16,38,'Tareas',22,True);text(384,47,'4 pendientes',11,anchor='ra',fill='#69736c')
        rows=[('Complete Q3 Report','Hoy · 14:00'),('Groceries','Milk, eggs, bread'),('Gym Session','Hoy · 18:30'),('Call Mom','Durante el día')]
        for i,(title,sub) in enumerate(rows):
            y=78+i*51
            box((16,y,384,y+45),8)
            box((29,y+14,46,y+31),4,outline='#67746c')
            text(62,y+5,title,14,True);text(62,y+25,sub,11,fill='#69736c')
    elif name=='calendar':
        d.rectangle((0,32*S,400*S,300*S),fill='white')
        text(26,65,'NOV 2023',18,True)
        for col,label in enumerate('SMTWTFS'):
            text(50+col*50,98,label,12,True,anchor='ma')
        for row,days in enumerate([(29,30,31,1,2,3,4),(5,6,7,8,9,10,11)]):
            for col,day in enumerate(days):
                x=50+col*50; y=122+row*30
                text(x,y,str(day),18,True,anchor='ma',fill='#555e57' if row==0 and col<3 else '#171e1a')
                if day in (2,8) and (row==1 or col==4):
                    line([(x-8,y+22),(x+8,y+22)],3)
        for x in (346,358,370):
            d.ellipse(((x-3)*S,80*S,(x+3)*S,86*S),fill='#171e1a')
        text(26,187,'NOV 8 · EVENTOS',13,True)
        columns=[('Recientes',[('Design Sync','09:00 – 10:00'),('Hardware Review','13:30 – 14:30')]),('Próximos',[('Daily equipo','15:00 – 15:30'),('Entrenamiento','Mañana · 18:30')]),('Importantes',[('Entrega Q3','Vie 27 · 14:00'),('Cumpleaños','13 junio')])]
        for i,(title,events) in enumerate(columns):
            x=26+i*123
            text(x,210,title,12,True)
            for j,(label,time) in enumerate(events):
                y=230+j*33
                line([(x,y+2),(x,y+27)],2)
                text(x+7,y,label,12,True);text(x+7,y+17,time,11,fill='#171e1a')
    elif name=='music':
        text(16,39,'Música',21,True);text(384,48,'REPRODUCIENDO',9,anchor='ra',fill='#69736c')
        portrait=source.crop((133,38,267,172)).resize((105*S,105*S),Image.Resampling.LANCZOS)
        box((16,80,139,203),14,fill='white')
        image.paste(portrait,(25*S,89*S))
        text(155,87,'Morning Light',25,True);text(155,125,'Studio Sessions',19,True)
        text(155,153,'ACOUSTIC / VOL. 01',12,fill='#171e1a')
        box((157,177,376,180),1,fill='#d7ded9',outline=None)
        box((157,177,238,180),1,fill='#171e1a',outline=None)
        text(157,185,'0:42',12,fill='#171e1a');text(376,185,'3:24',12,anchor='ra',fill='#171e1a')
        for cx,kind in [(135,'previous'),(200,'pause'),(265,'next')]:
            radius=25 if cx==200 else 19
            box((cx-radius,222-radius,cx+radius,222+radius),radius,fill='white',outline='#171e1a' if cx==200 else '#d1d9d3')
            icon(kind,cx-11,211,22)
        text(200,272,'ARCHIVO LOCAL · DEMOSTRACIÓN',9,anchor='ma',fill='#69736c')
    return image

def rest_view(home):
    image = Image.new('RGB', (400*S,300*S), 'white')
    portrait = home.crop((105*S,55*S,295*S,245*S)).resize((210*S,210*S),Image.Resampling.LANCZOS)
    mask=Image.new('L',(210*S,210*S),0)
    ImageDraw.Draw(mask).ellipse((2*S,2*S,208*S,208*S),fill=255)
    image.paste(portrait,(95*S,8*S),mask)
    font=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',48*S)
    ImageDraw.Draw(image).text((200*S,223*S),'07:30',font=font,fill='#171e1a',anchor='ma')
    return image
