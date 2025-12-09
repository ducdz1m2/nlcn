from flask import Flask, request, render_template, redirect, url_for
import os
import json
from werkzeug.utils import secure_filename
from flask import send_file

app = Flask(__name__)
UPLOAD_FOLDER = 'static'
META_FILE = 'meta.json'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# Tạo file meta nếu chưa có
if not os.path.exists(META_FILE):
    with open(META_FILE, 'w') as f:
        json.dump({}, f)

def load_meta():
    with open(META_FILE) as f:
        return json.load(f)

def save_meta(meta):
    with open(META_FILE, 'w') as f:
        json.dump(meta, f)

@app.route('/')
def index():
    files = os.listdir(UPLOAD_FOLDER)
    return render_template('index.html', files=files)

@app.route('/upload', methods=['POST'])
def upload():
    f = request.files['file']
    width = request.form.get('width', type=int)
    height = request.form.get('height', type=int)

    if f.filename and width and height:
        save_path = os.path.join(UPLOAD_FOLDER, f.filename)
        f.save(save_path)

        meta = load_meta()
        meta[f.filename] = {"width": width, "height": height}
        save_meta(meta)

        return f"Upload thành công! <a href='/'>Quay lại</a>"
    else:
        return "Thiếu thông tin file hoặc kích thước!"

@app.route('/preview/<filename>')
def preview(filename):
    meta = load_meta()
    info = meta.get(filename, {"width": 128, "height": 128})
    return render_template('preview.html', filename=filename, width=info["width"], height=info["height"])

@app.route('/delete/<filename>', methods=['POST'])
def delete(filename):
    file_path = os.path.join(UPLOAD_FOLDER, filename)

    if os.path.exists(file_path):
        os.remove(file_path)

    meta = load_meta()
    if filename in meta:
        del meta[filename]
        save_meta(meta)

    return redirect(url_for('index'))

@app.route('/cmd', methods=['POST'])
def cmd():
    command = request.form['command']
    print("Lệnh nhận được:", command)
    return f"Lệnh '{command}' đã được ghi nhận. <a href='/'>Quay lại</a>"



if __name__ == '__main__':
    app.run(host='0.0.0.0', port=3999, debug=True)

