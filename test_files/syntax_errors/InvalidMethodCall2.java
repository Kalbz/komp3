public class InvalidMethodCall2 {
    public static void main(String[] a) {
        System.out.println(new MyClass().Init());
    }
}

class MyClass {
    public boolean Init(){
        int test;

        test = this.Bar(,a); //@error - syntax (invalid comma at the begining of the list of parameters)
        return false;
    }

    public int Bar(int a, boolean b) {
        return 1;
    }
}
