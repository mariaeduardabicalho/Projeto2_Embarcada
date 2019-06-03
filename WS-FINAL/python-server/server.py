from flask import Flask, request
from flask_restful import Resource, Api

app = Flask(__name__)
api = Api(app)

todos = {}

# class HelloWorld(Resource):
#     def get(self):
#         return {'hello': 'world'}

class TodoSimple(Resource):
   def get(self, todo_id):
       return {todo_id: todos[todo_id]}

   def put(self, todo_id):


       dispositivo = request.json['dispositivo']
       id_sensor = request.json['id']
       value = request.json['value']
       timestamp = request.json['timestamp']


       print(" -------------------------------------------------------------\n")
       print("Dispositivo conectado: ")
       print(dispositivo)   
       print("\nID do sensor:")
       print(id_sensor)   
       print("\nValor do sensor:")
       print(value) 
       print("Timestamp:")
       print(timestamp)    
       #return {todo_id: todos[ph]}

api.add_resource(TodoSimple, '/<string:todo_id>')
# api.add_resource(HelloWorld, '/')

if __name__ == '__main__':
    app.run(host='0.0.0.0',debug=True)
    #app.run(debug=True)

