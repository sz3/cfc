
def get_path(name):
    fns = {
        'index': 'web/index.html',
        'cimbar_js': 'web/cimbar_js.js',
        'main_js': 'web/main.js',
        'send_js': 'web/send.js',
        'output': 'web/cimbar_js.html',
    }
    return fns[name]


def read_file(name):
    with open(get_path(name), 'rt') as f:
        return f.read()


def read_script(name):
    script = read_file(name)
    return '<script type="text/javascript">\n' + script + '\n'


def main():
    contents = read_file('index')
    send_js = read_script('send_js')
    main_js = read_script('main_js')
    cimbar_js = read_script('cimbar_js')

    contents = contents.replace('<script src="send.js">', send_js)
    contents = contents.replace('<script src="main.js">', main_js)
    contents = contents.replace('<script src="cimbar_js.js">', cimbar_js)

    with open(get_path('output'), 'wt') as f:
        f.write(contents)



if __name__ == '__main__':
    main()
