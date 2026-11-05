import socket
import time
import network
import dht
import json
from math import log, exp
from machine import ADC, Pin
 
# CONEXÃO WI-FI
 
def conecta_wifi(ssid, senha):
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
 
    if not wlan.isconnected():
        print("A conectar ao Wi-Fi...")
        wlan.connect(ssid, senha)
 
        while not wlan.isconnected():
            time.sleep_ms(500)
 
    print("Conexão estabelecida com sucesso!")
    print("Configurações de rede:", wlan.ifconfig())
 
    return wlan.ifconfig()[0]
ip = conecta_wifi("Garcias", "123456789")
 
#BASE MQ135
 
class BaseMQ(object):
 
    AMOSTRAS_MQ = const(5)
    INTERVALO_AMOSTRA_MQ = const(200)
 
    ESTRATEGIA_RAPIDA = const(1)
    ESTRATEGIA_PRECISA = const(2)
 
    def __init__(
        self,
        pino_dados,
        pino_aquecedor=-1,
        resistencia_placa=10,
        tensao_base=3.3,
        estrategia_medicao=ESTRATEGIA_PRECISA
    ):
        self._aquecedor = False
        self._resfriador = False
        self._ro = -1
        self._tensao_base = tensao_base
        self._resistencia_placa = resistencia_placa
 
        self.dados_confiaveis = False
        self._cache_rs = None
        self._estado_calibrado = False
 
        self.estrategia_medicao = estrategia_medicao
 
        self.pino_dados = ADC(Pin(pino_dados))
        self.pino_dados.atten(ADC.ATTN_11DB)
 
        if pino_aquecedor != -1:
            self._aquecedor = True
            self.pino_aquecedor = Pin(
                pino_aquecedor,
                Pin.OUT
            )
    def obter_ro_em_ar_limpo(self):
        raise NotImplementedError(
            "Implemente este método"
        )
    def calibrar(self, ro=-1):
        if ro == -1:
            ro = 0
 
            print("Calibrando MQ135...")
            print("Mantenha o sensor em ambiente limpo.")
 
            for i in range(1, self.AMOSTRAS_MQ + 1):
 
                print(
                    "Passo {}/{}".format(
                        i,
                        self.AMOSTRAS_MQ
                    )
                )
 
                leitura = self.pino_dados.read_u16()
 
                ro += self._calcular_resistencia_(leitura)
 
                time.sleep_ms(
                    self.INTERVALO_AMOSTRA_MQ
                )
 
            ro = (
                ro / self.AMOSTRAS_MQ
            ) / self.obter_ro_em_ar_limpo()
 
        self._ro = ro
        self._estado_calibrado = True
 
    def _calcular_resistencia_(self, adc_bruto):
 
        vrl = adc_bruto * (
            self._tensao_base / 65535.0
        )
 
        if vrl <= 0.001:
            vrl = 0.001
 
        rs_ar = (
            (self._tensao_base - vrl) / vrl
        ) * self._resistencia_placa
 
        return rs_ar
 
    def _ler_rs_(self):
 
        if self.estrategia_medicao == self.ESTRATEGIA_PRECISA:
 
            rs = 0
 
            for i in range(self.AMOSTRAS_MQ):
 
                leitura = self.pino_dados.read_u16()
 
                rs += self._calcular_resistencia_(
                    leitura
                )
 
                time.sleep_ms(50)
 
            rs = rs / self.AMOSTRAS_MQ
 
            self._cache_rs = rs
            self.dados_confiaveis = True
 
        else:
 
            leitura = self.pino_dados.read_u16()
 
            rs = self._calcular_resistencia_(
                leitura
            )
 
            self.dados_confiaveis = False
 
        return rs
 
    def ler_escalonado(self, a, b):
 
        razao = self.ler_razao()
 
        if razao <= 0:
            return 0.0
 
        val = (log(razao) - b) / a
 
        if val > 70 or val < -70:
            return 0.0
 
        return exp(val)
 
    def ler_razao(self):
 
        if self._ro <= 0:
            return 1.0
 
        return self._ler_rs_() / self._ro
# CLASSE MQ135
class MQ135(BaseMQ):
 
    RO_BASE_MQ135 = float(3.6)
 
    def __init__(
        self,
        pino_dados,
        pino_aquecedor=-1,
        resistencia_placa=10,
        tensao_base=3.3,
        estrategia_medicao=BaseMQ.ESTRATEGIA_PRECISA
    ):
 
        super().__init__(
            pino_dados,
            pino_aquecedor,
            resistencia_placa,
            tensao_base,
            estrategia_medicao
        )
 
    def ler_co2(self):
        return self.ler_escalonado(-0.42, 1.89)
 
    def ler_amonia(self):
        return self.ler_escalonado(-0.42, 1.15)
 
    def ler_alcool(self):
        return self.ler_escalonado(-0.40, 1.34)
 
    def ler_benzeno(self):
        return self.ler_escalonado(-0.38, 1.23)
 
    def obter_ro_em_ar_limpo(self):
        return self.RO_BASE_MQ135
# INICIALIZAÇÃO DOS SENSORES
 
print("Inicializando sensores...")
 
# Sensor DHT22 - GPIO 4
dht_sensor = dht.DHT22(Pin(4))
 
# Sensor MQ135 - GPIO 34
sensor_mq135 = MQ135(
    pino_dados=34,
    tensao_base=3.3
)
 
print("Iniciando calibração do MQ135...")
 
sensor_mq135.calibrar()
 
print("Calibração completa!")
 
print(
    "Resistência Base (Ro): {:.2f}".format(
        sensor_mq135._ro
    )
)
 
# Sensor LDR - GPIO 35
ldr_adc = ADC(Pin(35))
ldr_adc.atten(ADC.ATTN_11DB)
 
# LEITURA DA LUMINOSIDADE
 
def ler_luminosidade():
 
    val_adc = ldr_adc.read_u16()
 
    lux = int(
        (val_adc / 65535.0) * 1200
    )
 
    return max(50, min(lux, 1200))
# LEITURA DA PRESSÃO
# Não altere nada da pressão
 
def ler_bmp280():
 
    # Valor provisório.
    # Substituir pela leitura real quando
    # o BMP280 estiver conectado.
 
    return 1012.0
 
# OBTER LEITURAS DOS SENSORES
# Leitura Ok
 
def obter_leituras():
 
    try:
 
        dht_sensor.measure()
 
        temp = dht_sensor.temperature()
        umi = dht_sensor.humidity()
 
    except Exception as e:
 
        print("Erro no DHT22:", e)
 
        temp = 24.3
        umi = 56.0
 
    try:
 
        mq = sensor_mq135.ler_co2()
 
    except Exception as e:
 
        print("Erro no MQ135:", e)
 
        mq = 0
 
    return {
        "temp": round(temp, 1),
        "umi": round(umi, 1),
        "mq": round(mq, 1),
        "pressao": ler_bmp280(),
        "lux": ler_luminosidade()
    }
 
# PÁGINA HTML
# Revisado html sem css
 
def pagina_html():
 
    return """<!DOCTYPE html>
<html>
<head>
 
<meta charset="UTF-8">
 
<title>MONITORAMENTO URBANO - Equipe tempo aberto</title>
 
<meta name="viewport" content="width=device-width, initial-scale=1">
 
<style>
 
body {
    font-family: Light Grey, sans-serif;
    background-color: #D3D3D3;
    margin: 0;
    padding: 0;
    text-align: center;
}
 
.header {
    background-color: #89CFF0;
    color: blue-baby;
    padding: 25px 10px;
    box-shadow: 0px 4px 6px rgba(0,0,0,0.1);
}
 
.header h1 {
    margin: 0;
    font-size: 24px;
}
 
.header h2 {
    margin: 5px 0 0 0;
    font-size: 18px;
    font-weight: normal;
}
 
.container {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    gap: 20px;
    padding: 30px 10px;
    max-width: 1000px;
    margin: 0 auto;
}
 
.card {
    background: Arial;
    border-radius: 12px;
    box-shadow: 0px 4px 12px rgba(0,0,0,0.08);
    padding: 20px;
    width: 210px;
    text-align: center;
}
 
.card h3 {
    margin: 0 0 10px 0;
    color: #333;
    font-size: 16px;
}
 
.value {
    font-size: 26px;
    font-weight: bold;
    color: #4289db;
    margin: 10px 0;
}
 
.footer-badge {
    display: inline-block;
    background: #ffe6e6;
    color: #b30000;
    padding: 4px 8px;
    border-radius: 6px;
    font-size: 12px;
    margin: 2px;
}
 
.footer-badge-blue {
    display: inline-block;
    background: #e6f2ff;
    color: #004080;
    padding: 4px 8px;
    border-radius: 6px;
    font-size: 12px;
    margin: 2px;
}
 
.footer {
    margin-top: 20px;
    color: #666;
    font-size: 12px;
}
 
</style>
 
<script>
 
function atualizarDados() {
 
    fetch('/dados')
 
    .then(response => {
 
        if (!response.ok) {
            throw new Error('Erro HTTP');
        }
 
        return response.json();
 
    })
 
    .then(data => {
 
        document.getElementById('temp').innerText =
            data.temp.toFixed(1) + ' °C';
 
        document.getElementById('umi').innerText =
            data.umi.toFixed(1) + ' %';
 
        document.getElementById('mq').innerText =
            data.mq.toFixed(1);
 
        document.getElementById('pressao').innerText =
            data.pressao.toFixed(0) + ' hPa';
 
        document.getElementById('lux').innerText =
            data.lux + ' lux';
 
    })
 
    .catch(error => {
 
        console.error('Erro ao atualizar:', error);
 
    });
 
}
 
// Atualiza imediatamente ao abrir a página
atualizarDados();
 
// Atualiza a cada 3 segundos
setInterval(atualizarDados, 3000);
 
</script>
 
</head>
 
<body>
 
<div class="header">
 
<h1>Monitoramento Urbano</h1>
 
<h2>Tempo Aberto</h2>
 
<p style="margin:5px 0 0 0; font-size:12px;">
Cidades Urbanas em Monitoramento
</p>
 
</div>
 
 
<div class="container">
 
    <div class="card">
 
        <h3>Temperatura</h3>
 
        <div id="temp" class="value">-- °C</div>
 
        <div>
<span class="footer-badge-blue">Mín: 18.1 °C</span>
<span class="footer-badge">Máx: 32.7 °C</span>
</div>
 
    </div>
 
 
    <div class="card">
 
        <h3>Umidade</h3>
 
        <div id="umi" class="value">-- %</div>
 
        <div>
<span class="footer-badge-blue">Mín: 35 %</span>
<span class="footer-badge">Máx: 92 %</span>
</div>
 
    </div>
 
 
    <div class="card">
 
        <h3>Poluição</h3>
 
        <div id="mq" class="value">--</div>
 
        <div>
<span class="footer-badge-blue">Mín: 120</span>
<span class="footer-badge">Máx: 680</span>
</div>
 
    </div>
 
 
    <div class="card">
 
        <h3>Pressão</h3>
 
        <div id="pressao" class="value">-- hPa</div>
 
        <div>
<span class="footer-badge-blue">Mín: 1008 hPa</span>
<span class="footer-badge">Máx: 1020 hPa</span>
</div>
 
    </div>
 
 
    <div class="card">
 
        <h3>Luminosidade</h3>
 
        <div id="lux" class="value">-- lux</div>
 
        <div>
<span class="footer-badge-blue">Mín: 50 lux</span>
<span class="footer-badge">Máx: 1200 lux</span>
</div>
 
    </div>
 
</div>
 
 
<div class="footer">
 
<p>Servidor Web ESP32 - Comunicação via API JSON</p>
 
</div>
 
</body>
</html>
"""
# SERVIDOR WEB ESP32
#Ta funcionando
s = socket.socket(
    socket.AF_INET,
    socket.SOCK_STREAM
)
 
s.setsockopt(
    socket.SOL_SOCKET,
    socket.SO_REUSEADDR,
    1
)
 
s.bind(('', 80))
 
s.listen(5)
 
print("--------------------------------")
print("Servidor Web iniciado!")
print("Acesse pelo navegador:")
print("http://" + ip)
print("--------------------------------")
 
# LOOP PRINCIPAL DO SERVIDOR
 
while True:
 
    conn = None
 
    try:
 
        conn, addr = s.accept()
 
        print("Conexão recebida de:", addr)
 
        request = conn.recv(1024).decode('utf-8')
 
        if "GET /dados" in request:
 
            dados = obter_leituras()
 
            resposta_json = json.dumps(dados)
 
            cabecalho = (
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Connection: close\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "\r\n"
            )
 
            conn.sendall(cabecalho.encode('utf-8'))
 
            conn.sendall(
                resposta_json.encode('utf-8')
            )
 
            print("Dados enviados:", dados)
 
        else:
 
            resposta = pagina_html()
 
            cabecalho = (
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n"
                "\r\n"
            )
 
            conn.sendall(cabecalho.encode('utf-8'))
 
            conn.sendall(
                resposta.encode('utf-8')
            )
 
    except Exception as e:
 
        print("Erro no servidor:", e)
 
    finally:
 
        if conn is not None:
 
            try:
                conn.close()
 
            except:
                pass
